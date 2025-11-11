#include "pch.h"
#include "PacketHandler.h"
#include "Session.h"
#include "GameLogic.h"
#include "Database.h"

/**
* This function dynamically allocates an OverlappedEx structure for sending.
* The allocated resource should be deleted in the WorkerThread's IOOperation::Send case after the send operation is complete.
*/
void SendPacketAsync(Session* pSession, char* pPacket, int size)
{
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
        delete pOverlappedEx;
    }
}

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
        SendPacketAsync(pSession, (char*)&res, res.packetLength);
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
        SendPacketAsync(pSession, (char*)&res, res.packetLength);
        break;
    }

    case PacketType::EnterRoomReq:
    {
        if (!pSession->isLoggedIn || pSession->currentRoomID != LOBBY_ID) break;

        PktEnterRoomReq* pReq = reinterpret_cast<PktEnterRoomReq*>(pPacketData);
        Room* pRoom = g_RoomManager.GetRoomOrNull(pReq->roomID);

        PktEnterRoomRes res; // res.success is false by default
        if (pRoom != nullptr)
        {
            if (pRoom->AddUser(pSession))
            {
                g_Lobby.RemoveUser(pSession);
                res.success = true;
                res.roomID = pRoom->GetID();
            }
        }
        SendPacketAsync(pSession, (char*)&res, res.packetLength);
        break;
    }

    case PacketType::EnterRandomRoomReq:
    {
        if (!pSession->isLoggedIn || pSession->currentRoomID != LOBBY_ID) break;

        Room* pRoom = g_RoomManager.GetRandomAvailableRoomOrNull();
        PktEnterRoomRes res;

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
        SendPacketAsync(pSession, (char*)&res, res.packetLength);
        break;
    }

    case PacketType::LeaveRoomReq:
    {
        if (!pSession->isLoggedIn || pSession->currentRoomID == LOBBY_ID) break;

        Room* pRoom = g_RoomManager.GetRoomOrNull(pSession->currentRoomID);
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
        SendPacketAsync(pSession, (char*)&res, res.packetLength);
        break;
    }

    case PacketType::ChatReq:
    {
        if (!pSession->isLoggedIn) break;

        PktChatReq* pReq = reinterpret_cast<PktChatReq*>(pPacketData);
        std::string message(pReq->message, strnlen_s(pReq->message, MAX_CHAT_LEN));

        PersistChatMessage(pSession->currentRoomID, pSession->userID, message); // store chat message in the database

        PktChatNtf ntf;
        strncpy_s(ntf.userID, pSession->userID.c_str(), MAX_USER_ID_LEN);
        strncpy_s(ntf.message, message.c_str(), MAX_CHAT_LEN);

        if (pSession->currentRoomID == LOBBY_ID)
        {
            g_Lobby.Broadcast((char*)&ntf, ntf.packetLength);
        }
        else
        {
            Room* pRoom = g_RoomManager.GetRoomOrNull(pSession->currentRoomID);
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


void ProcessRecv(Session* pSession, DWORD bytesTransferred)
{
    memcpy(pSession->packetBuffer + pSession->currentPacketSize,
        pSession->recvOverlapped.buffer,
        bytesTransferred);

    pSession->currentPacketSize += bytesTransferred;

    // parsing loop
    while (pSession->currentPacketSize >= sizeof(PacketHeader))
    {
        PacketHeader* pHeader = reinterpret_cast<PacketHeader*>(pSession->packetBuffer);

        if (pSession->currentPacketSize >= pHeader->packetLength)
        {
            ProcessPacket(pSession, pSession->packetBuffer);

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
            // packet is not fully received yet
            break;
        }
    }
}
