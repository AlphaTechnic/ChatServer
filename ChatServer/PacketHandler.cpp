#include "pch.h"
#include "PacketHandler.h"
#include "Session.h"
#include "GameLogic.h"  // g_Lobby, g_RoomManager
#include "Database.h" // LogChatMessage

/**
 * @brief 비동기 Send 요청
 */
void PostSend(Session* pSession, char* pPacket, int size)
{
    // Send용 OverlappedEx는 동적 할당 (Send 완료 시 해제)
    OverlappedEx* pOverlappedEx = new OverlappedEx();
    ZeroMemory(pOverlappedEx, sizeof(OverlappedEx));

    pOverlappedEx->operation = IOOperation::Send;
    memcpy(pOverlappedEx->buffer, pPacket, size);
    pOverlappedEx->wsaBuf.buf = pOverlappedEx->buffer;
    pOverlappedEx->wsaBuf.len = size;

    int sendResult = WSASend(
        pSession->socket,
        &(pOverlappedEx->wsaBuf),
        1,
        NULL,
        0,
        &(pOverlappedEx->overlapped),
        NULL
    );

    if (sendResult == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
    {
        std::cerr << "WSASend failed immediately! (Socket: " << pSession->socket << ") Error: " << WSAGetLastError() << std::endl;
        delete pOverlappedEx; // 실패 시에도 Send 완료 통지는 오지 않으므로 여기서 delete
    }
}

/**
 * @brief 완성된 패킷을 처리하는 함수
 */
void ProcessPacket(Session* pSession, char* pPacketData)
{
    PacketHeader* pHeader = reinterpret_cast<PacketHeader*>(pPacketData);

    switch (pHeader->type)
    {
    case PacketType::LoginReq:
    {
        PktLoginReq* pReq = reinterpret_cast<PktLoginReq*>(pPacketData);
        pSession->userID = std::string(pReq->userID, strnlen_s(pReq->userID, MAX_USER_ID_LEN));
        pSession->isLoggedIn = true;

        std::cout << "[System] User '" << pSession->userID << "' logged in." << std::endl;
        g_Lobby.AddUser(pSession);

        PktLoginRes res;
        res.success = true;
        PostSend(pSession, (char*)&res, res.packetLength);
        break;
    }

    case PacketType::CreateRoomReq:
    {
        if (!pSession->isLoggedIn || pSession->currentRoomID != LOBBY_ID) break;

        g_Lobby.RemoveUser(pSession);
        Room* pNewRoom = g_RoomManager.CreateRoom();
        pNewRoom->AddUser(pSession);

        PktCreateRoomRes res;
        res.success = true;
        res.newRoomID = pNewRoom->GetID();
        PostSend(pSession, (char*)&res, res.packetLength);
        break;
    }

    case PacketType::EnterRoomReq:
    {
        if (!pSession->isLoggedIn || pSession->currentRoomID != LOBBY_ID) break;

        PktEnterRoomReq* pReq = reinterpret_cast<PktEnterRoomReq*>(pPacketData);
        Room* pRoom = g_RoomManager.GetRoom(pReq->roomID);

        PktEnterRoomRes res; // success는 기본 false
        if (pRoom != nullptr)
        {
            if (pRoom->AddUser(pSession))
            {
                g_Lobby.RemoveUser(pSession);
                res.success = true;
                res.roomID = pRoom->GetID();
            }
        }
        PostSend(pSession, (char*)&res, res.packetLength);
        break;
    }

    case PacketType::EnterRandomRoomReq:
    {
        if (!pSession->isLoggedIn || pSession->currentRoomID != LOBBY_ID) break;

        Room* pRoom = g_RoomManager.GetRandomAvailableRoomOrNull();
        PktEnterRoomRes res; // 응답은 EnterRoomRes와 동일

        if (pRoom == nullptr)
        {
            std::cout << "[System] No available rooms for random entry for User '" << pSession->userID << "'." << std::endl;
        }
        else
        {
            if (pRoom->AddUser(pSession))
            {
                g_Lobby.RemoveUser(pSession);
                res.success = true;
                res.roomID = pRoom->GetID();
            }
            else
            {
                std::cout << "[System] Random entry failed (Race Condition) for User '" << pSession->userID << "'." << std::endl;
            }
        }
        PostSend(pSession, (char*)&res, res.packetLength);
        break;
    }

    case PacketType::LeaveRoomReq:
    {
        if (!pSession->isLoggedIn || pSession->currentRoomID == LOBBY_ID) break;

        Room* pRoom = g_RoomManager.GetRoom(pSession->currentRoomID);
        if (pRoom)
        {
            pRoom->RemoveUser(pSession);
            if (pRoom->GetUserCount() == 0)
            {
                g_RoomManager.RemoveRoom(pRoom->GetID());
            }
        }

        g_Lobby.AddUser(pSession);

        PktLeaveRoomRes res;
        res.success = true;
        PostSend(pSession, (char*)&res, res.packetLength);
        break;
    }

    case PacketType::ChatReq:
    {
        if (!pSession->isLoggedIn) break;

        PktChatReq* pReq = reinterpret_cast<PktChatReq*>(pPacketData);
        std::string message(pReq->message, strnlen_s(pReq->message, MAX_CHAT_LEN));

        // DB 로그 저장
        LogChatMessage(pSession->currentRoomID, pSession->userID, message);

        PktChatNtf ntf;
        strncpy_s(ntf.userID, pSession->userID.c_str(), MAX_USER_ID_LEN);
        strncpy_s(ntf.message, message.c_str(), MAX_CHAT_LEN);

        if (pSession->currentRoomID == LOBBY_ID)
        {
            g_Lobby.Broadcast((char*)&ntf, ntf.packetLength);
        }
        else
        {
            Room* pRoom = g_RoomManager.GetRoom(pSession->currentRoomID);
            if (pRoom)
            {
                pRoom->Broadcast((char*)&ntf, ntf.packetLength);
            }
        }
        break;
    }

    default:
        std::cerr << "Unknown packet type: " << static_cast<int>(pHeader->type) << std::endl;
        break;
    }
}


/**
 * @brief 수신된 데이터를 파싱하고 패킷을 조립/처리하는 함수
 */
void ProcessRecv(Session* pSession, DWORD bytesTransferred)
{
    // 수신한 데이터를 세션의 패킷 버퍼 뒤에 이어 붙임
    memcpy(pSession->packetBuffer + pSession->currentPacketSize,
        pSession->recvOverlapped.buffer,
        bytesTransferred);

    pSession->currentPacketSize += bytesTransferred;

    // 패킷 조립(Parsing) 루프
    while (pSession->currentPacketSize >= sizeof(PacketHeader))
    {
        PacketHeader* pHeader = reinterpret_cast<PacketHeader*>(pSession->packetBuffer);

        if (pSession->currentPacketSize >= pHeader->packetLength)
        {
            // 3. 패킷이 완성됨 -> 처리
            ProcessPacket(pSession, pSession->packetBuffer);

            // 4. 처리한 패킷 크기만큼 버퍼에서 제거
            int remainingSize = pSession->currentPacketSize - pHeader->packetLength;
            if (remainingSize > 0)
            {
                memmove(pSession->packetBuffer,
                    pSession->packetBuffer + pHeader->packetLength,
                    remainingSize);
            }
            pSession->currentPacketSize = remainingSize;
        }
        else
        {
            // 5. 헤더는 왔지만 데이터가 아직 덜 옴
            break;
        }
    }
}
