#include "pch.h"
#include "PacketHandler.h"

namespace PacketHandler
{
	void ProcessPacket(char* pPacketData)
	{
		PacketHeader* pHeader = reinterpret_cast<PacketHeader*>(pPacketData);

		switch (pHeader->type)
		{
		case PacketType::LoginRes:
		{
			PktLoginRes* pRes = reinterpret_cast<PktLoginRes*>(pPacketData);
			if (pRes->success)
                std::cout << "[System] Login successful. You have entered the lobby." << std::endl;
			else
                std::cout << "[System] Login failed. The user ID may already be in use." << std::endl;
			break;
		}
		case PacketType::CreateRoomRes:
		{
			PktCreateRoomRes* pRes = reinterpret_cast<PktCreateRoomRes*>(pPacketData);
			if (pRes->success)
                std::cout << "[System] Room created successfully. (Room ID: " << pRes->newRoomID << ")" << std::endl;
			else
			    std::cout << "[System] Room creation failed." << std::endl;
			break;
		}
		case PacketType::EnterRoomRes:
		{
			PktEnterRoomRes* pRes = reinterpret_cast<PktEnterRoomRes*>(pPacketData);
			if (pRes->success)
                std::cout << "[System] Successfully entered the room. (Room ID: " << pRes->roomID << ")" << std::endl;
			else
                std::cout << "[System] Failed to enter the room (e.g., room does not exist or is full)." << std::endl;
			break;
		}
		case PacketType::LeaveRoomRes:
		{
			PktLeaveRoomRes* pRes = reinterpret_cast<PktLeaveRoomRes*>(pPacketData);
            std::cout << "[System] You have left the room and returned to the lobby." << std::endl;
			break;
		}
		case PacketType::ChatNtf:
		{
			PktChatNtf* pNtf = reinterpret_cast<PktChatNtf*>(pPacketData);
			std::cout << "[" << pNtf->userID << "] " << pNtf->message << std::endl;
			break;
		}
		case PacketType::UserEnterNtf:
		{
			PktUserEnterNtf* pNtf = reinterpret_cast<PktUserEnterNtf*>(pPacketData);
            std::cout << "[System] '" << pNtf->userID << "' has entered." << std::endl;
			break;
		}
		case PacketType::UserLeaveNtf:
		{
			PktUserLeaveNtf* pNtf = reinterpret_cast<PktUserLeaveNtf*>(pPacketData);
            std::cout << "[System] '" << pNtf->userID << "' has left." << std::endl;
			break;
		}
		case PacketType::UserListNtf:
		{
			PktUserListNtf* pNtf = reinterpret_cast<PktUserListNtf*>(pPacketData);
			short userCount = pNtf->userCount;

            std::cout << "[System] --- Current Users Online (" << userCount << " users) ---" << std::endl;

			char* pData = (char*)(pNtf + 1);

			for (short i = 0; i < userCount; ++i)
			{
				std::string currentUserID(pData, strnlen_s(pData, MAX_USER_ID_LEN));
                std::cout << "[System] - " << currentUserID << std::endl;
				pData += MAX_USER_ID_LEN;
			}
			std::cout << "[System] ---------------------------------" << std::endl;
			break;
		}
		default:
			std::cerr << "Unknown packet type received: " << static_cast<int>(pHeader->type) << std::endl;
			break;
		}
	}

}
