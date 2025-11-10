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
				std::cout << "[System] 로그인 성공. 로비에 입장했습니다." << std::endl;
			else
				std::cout << "[System] 로그인 실패." << std::endl;
			break;
		}
		case PacketType::CreateRoomRes:
		{
			PktCreateRoomRes* pRes = reinterpret_cast<PktCreateRoomRes*>(pPacketData);
			if (pRes->success)
				std::cout << "[System] 방 생성 성공. (Room ID: " << pRes->newRoomID << ")" << std::endl;
			else
				std::cout << "[System] 방 생성 실패." << std::endl;
			break;
		}
		case PacketType::EnterRoomRes:
		{
			PktEnterRoomRes* pRes = reinterpret_cast<PktEnterRoomRes*>(pPacketData);
			if (pRes->success)
				std::cout << "[System] 방 입장 성공. (Room ID: " << pRes->roomID << ")" << std::endl;
			else
				std::cout << "[System] 방 입장 실패 (예: 방이 없거나, 방이 꽉 찼음)." << std::endl;
			break;
		}
		case PacketType::LeaveRoomRes:
		{
			PktLeaveRoomRes* pRes = reinterpret_cast<PktLeaveRoomRes*>(pPacketData);
			std::cout << "[System] 방을 떠나 로비로 돌아왔습니다." << std::endl;
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
			std::cout << "[System] '" << pNtf->userID << "' 님이 입장했습니다." << std::endl;
			break;
		}
		case PacketType::UserLeaveNtf:
		{
			PktUserLeaveNtf* pNtf = reinterpret_cast<PktUserLeaveNtf*>(pPacketData);
			std::cout << "[System] '" << pNtf->userID << "' 님이 퇴장했습니다." << std::endl;
			break;
		}
		case PacketType::UserListNtf:
		{
			PktUserListNtf* pNtf = reinterpret_cast<PktUserListNtf*>(pPacketData);
			short userCount = pNtf->userCount;

			std::cout << "[System] --- 현재 접속 중인 유저 (" << userCount << "명) ---" << std::endl;

			char* pData = (char*)(pNtf + 1);

			for (short i = 0; i < userCount; ++i)
			{
				std::string currentUserID(pData, strnlen_s(pData, MAX_USER_ID_LEN));
				std::cout << "[System] - " << currentUserID << " 님" << std::endl;
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
