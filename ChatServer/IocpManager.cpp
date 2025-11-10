#include "pch.h"
#include "IocpManager.h"
#include "Session.h"
#include "GameLogic.h"
#include "Database.h"
#include "PacketHandler.h"

namespace IocpManager
{
    static HANDLE g_iocpHandle;
    static std::vector<std::thread> g_workerThreads;
    static std::atomic<bool> g_isServerRunning = true;

    static std::unordered_map<SOCKET, Session*> g_sessions;
    static std::mutex g_sessionMutex;

    static void WorkerThread();
    static void TimeoutThread();

    bool InitIocp(int threadCount)
    {
        g_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (g_iocpHandle == NULL)
        {
            std::cerr << "CreateIoCompletionPort failed!" << std::endl;
            return false;
        }
        return true;
    }

    void StartWorkerThreads(int threadCount)
    {
        g_isServerRunning = true;
        for (int i = 0; i < threadCount; ++i)
        {
            g_workerThreads.emplace_back(WorkerThread);
        }
        std::cout << "Server :: " << threadCount << " worker threads created." << std::endl;
    }

    void StartTimeoutThread()
    {
        std::thread(TimeoutThread).detach();
    }

    void StopIocp()
    {
        g_isServerRunning = false;

        for (size_t i = 0; i < g_workerThreads.size(); ++i)
        {
            PostQueuedCompletionStatus(g_iocpHandle, 0, NULL, NULL);
        }

        for (auto& th : g_workerThreads)
        {
            if (th.joinable())
            {
                th.join();
            }
        }

        CloseHandle(g_iocpHandle);
    }

    HANDLE GetIocpHandle()
    {
        return g_iocpHandle;
    }


    void AddSession(Session* pSession)
    {
        std::lock_guard<std::mutex> lock(g_sessionMutex);
        g_sessions[pSession->socket] = pSession;
    }

    void RemoveSession(SOCKET socket, Session* pSession)
    {
        // 1. remove from game logic
        if (pSession->isLoggedIn)
        {
            if (pSession->currentRoomID == LOBBY_ID)
            {
                g_Lobby.RemoveUser(pSession);
            }
            else
            {
                Room* pRoom = g_RoomManager.GetRoomOrNull(pSession->currentRoomID);
                if (pRoom)
                {
                    pRoom->RemoveUser(pSession);
                    if (pRoom->GetUserCount() == 0)
                    {
                        g_RoomManager.RemoveRoom(pRoom->GetID());
                    }
                }
            }
        }

        // 2. remove from global session map
        {
            std::lock_guard<std::mutex> lock(g_sessionMutex);
            g_sessions.erase(socket);
        }

        // 3. release resources
        closesocket(pSession->socket);
        delete pSession;
    }


    static void WorkerThread()
    {
        DWORD bytesTransferred;
        ULONG_PTR completionKey;
        LPOVERLAPPED lpOverlapped;

        std::cout << "[Debug] Worker Thread " << std::this_thread::get_id() << " started." << std::endl;

        while (g_isServerRunning)
        {
            bool result = GetQueuedCompletionStatus(
                g_iocpHandle,
                &bytesTransferred,
                &completionKey,
                &lpOverlapped,
                INFINITE
            );

            Session* pSession = reinterpret_cast<Session*>(completionKey);
            if (pSession == nullptr)
            {
                std::cerr << "Worker thread " << std::this_thread::get_id() << " exiting..." << std::endl;
                break;
            }

            OverlappedEx* pOverlappedEx = reinterpret_cast<OverlappedEx*>(lpOverlapped);

            if (!result || bytesTransferred == 0)
            {
                if (!result)
                {
                    std::cout << "Client disconnected (Socket Error " << WSAGetLastError() << ", User: " << pSession->userID << ")" << std::endl;
                }
                else
                {
                    std::cout << "Client disconnected (Graceful, User: " << pSession->userID << ")" << std::endl;
                }

                RemoveSession(pSession->socket, pSession);
                continue;
            }

            switch (pOverlappedEx->operation)
            {
            case IOOperation::Recv:
            {
                pSession->lastActivityTime = std::chrono::steady_clock::now();
                ProcessRecv(pSession, bytesTransferred);

                DWORD recvBytes = 0;
                DWORD flags = 0;
                ZeroMemory(&(pSession->recvOverlapped.overlapped), sizeof(OVERLAPPED));
                pSession->recvOverlapped.wsaBuf.len = MAX_BUFFER_SIZE;

                int recvResult = WSARecv(
                    pSession->socket,
                    &(pSession->recvOverlapped.wsaBuf),
                    1,
                    &recvBytes,
                    &flags,
                    &(pSession->recvOverlapped.overlapped),
                    NULL
                );

                if (recvResult == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
                {
                    std::cerr << "WSARecv failed after processing! Error: " << WSAGetLastError() << std::endl;
                }
                break;
            }

            case IOOperation::Send:
            {
                delete pOverlappedEx;
                break;
            }
            }
        }
    }

    static void TimeoutThread()
    {
        std::cout << "[Debug] Timeout Thread " << std::this_thread::get_id() << " started." << std::endl;

        auto lastDbCleanupTime = std::chrono::steady_clock::now();

        while (g_isServerRunning)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT_CHECK_INTERVAL_MS));

            if (!g_isServerRunning) break;

            auto now = std::chrono::steady_clock::now();

            auto elapsedSinceCleanup = std::chrono::duration_cast<std::chrono::hours>(now - lastDbCleanupTime);
            if (elapsedSinceCleanup.count() >= DB_CLEANUP_INTERVAL_HOURS)
            {
                CleanupOldChatLogs();
                lastDbCleanupTime = now;
            }

            std::vector<SOCKET> timedOutSockets;
            {
                std::lock_guard<std::mutex> lock(g_sessionMutex);
                if (g_sessions.empty()) continue;

                for (auto& pair : g_sessions)
                {
                    Session* pSession = pair.second;
                    auto lastActivity = pSession->lastActivityTime.load();
                    auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - lastActivity).count();

                    if (elapsedSeconds > SESSION_TIMEOUT_SECONDS)
                    {
                        timedOutSockets.push_back(pSession->socket);
                    }
                }
            }

            if (!timedOutSockets.empty())
            {
                std::cout << "[Timeout] Disconnecting " << timedOutSockets.size() << " inactive clients..." << std::endl;
                for (SOCKET sock : timedOutSockets)
                {
                    closesocket(sock);
                }
            }
        }
        std::cout << "[Debug] Timeout Thread " << std::this_thread::get_id() << " exiting." << std::endl;
    }
}
