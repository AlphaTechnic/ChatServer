#include "pch.h"
#include "IocpManager.h"
#include "Session.h"

int main()
{
    // initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed!" << std::endl;
        return 1;
    }

    // create IOCP port
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    int threadCount = sysInfo.dwNumberOfProcessors;

    if (!IocpManager::InitIocp(threadCount))
    {
        WSACleanup();
        return 1;
    }

    // start worker threads and timeout thread
    IocpManager::StartWorkerThreads(threadCount);
    IocpManager::StartTimeoutThread();

    // create listen socket
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET)
    {
        std::cerr << "Listen socket creation failed!" << std::endl;
        WSACleanup();
        return 1;
    }

    // setup server address and bind
    sockaddr_in serverAddr = { 0, };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(SERVER_PORT);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cerr << "Socket bind failed!" << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    // start to listen
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cerr << "Socket listen failed!" << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server :: Chat server listening on port " << SERVER_PORT << "..." << std::endl;

    // accept loop for incoming client connections
    while (true)
    {
        sockaddr_in clientAddr = { 0, };
        int addrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &addrLen);

        if (clientSocket == INVALID_SOCKET)
        {
            std::cerr << "Accept failed!" << std::endl;
            continue;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, INET_ADDRSTRLEN);
        std::cout << "Client connected from " << clientIp << ":" << ntohs(clientAddr.sin_port) << std::endl;

        Session* pSession = new Session(clientSocket);
        IocpManager::AddSession(pSession);
        // pass pSession as CompletionKey
        CreateIoCompletionPort((HANDLE)clientSocket, IocpManager::GetIocpHandle(), (ULONG_PTR)pSession, 0);

        DWORD recvBytes = 0;
        DWORD flags = 0;
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
            std::cerr << "WSARecv failed immediately!" << std::endl;
            IocpManager::RemoveSession(clientSocket, pSession);
        }
    }

    // cleanup
    IocpManager::StopIocp();
    closesocket(listenSocket);
    WSACleanup();
    return 0;
}
