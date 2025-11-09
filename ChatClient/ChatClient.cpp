#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <string>

#include "Protocol.h"

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1" // 서버 IP (localhost

/**
 * @brief 서버로부터 받은 패킷을 처리하는 함수
 */
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
			// [랜덤 입장 추가] 서버가 EnterRoomRes(false)로 응답하므로 이 메시지가 출력됨.
			std::cout << "[System] 방 입장 실패 (예: 방이 없거나, 꽉 찼거나, 입장 가능한 방이 없음)." << std::endl;
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
		// (요구사항) 채팅 브로드캐스팅 수신
		PktChatNtf* pNtf = reinterpret_cast<PktChatNtf*>(pPacketData);
		std::cout << "[" << pNtf->userID << "] " << pNtf->message << std::endl;
		break;
	}
	// TODO: (요구사항) UserEnterNtf, UserLeaveNtf 처리
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
	// (v7 수정) "초기 리스트" 수신 (가변 길이 패킷 파싱)
	case PacketType::UserListNtf:
	{
		PktUserListNtf* pNtf = reinterpret_cast<PktUserListNtf*>(pPacketData);
		short userCount = pNtf->userCount;

		std::cout << "[System] --- 현재 접속 중인 유저 (" << userCount << "명) ---" << std::endl;

		// (중요) 헤더 바로 뒤(데이터 영역) 포인터 획득
		char* pData = (char*)(pNtf + 1);

		for (short i = 0; i < userCount; ++i)
		{
			// pData에서 MAX_USER_ID_LEN 만큼 읽음
			std::string currentUserID(pData, strnlen_s(pData, MAX_USER_ID_LEN));
			std::cout << "[System] - " << currentUserID << " 님" << std::endl;

			// (요구사항) 유저 리스트 자료구조에 currentUserID 추가

			// 다음 ID 위치로 포인터 이동
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


/**
 * @brief (Recv 스레드) 서버로부터 패킷을 수신하는 스레드
 */
void RecvThread(SOCKET serverSocket)
{
	char recvBuffer[MAX_BUFFER_SIZE]; // 서버에서 받은 원시 데이터
	char packetBuffer[MAX_BUFFER_SIZE * 2]; // 패킷 조립용 버퍼
	int currentPacketSize = 0;

	std::cout << "[Debug] Recv thread started." << std::endl;

	while (true)
	{
		// 1. 데이터 수신 (Blocking)
		int nRecv = recv(serverSocket, recvBuffer, MAX_BUFFER_SIZE, 0);
		if (nRecv <= 0)
		{
			// 0: 서버가 정상 종료, -1: 소켓 오류
			std::cout << "[System] 서버와 연결이 끊어졌습니다." << std::endl;
			closesocket(serverSocket);
			break;
		}

		// 2. 수신한 데이터를 패킷 조립 버퍼에 복사
		memcpy(packetBuffer + currentPacketSize, recvBuffer, nRecv);
		currentPacketSize += nRecv;

		// 3. 패킷 조립 (서버의 ProcessRecv와 동일한 로직)
		while (currentPacketSize >= sizeof(PacketHeader))
		{
			PacketHeader* pHeader = reinterpret_cast<PacketHeader*>(packetBuffer);
			if (currentPacketSize >= pHeader->packetLength)
			{
				// 패킷 완성 -> 처리
				ProcessPacket(packetBuffer);

				// 처리한 패킷만큼 버퍼에서 제거
				int remainingSize = currentPacketSize - pHeader->packetLength;
				if (remainingSize > 0)
				{
					memmove(packetBuffer, packetBuffer + pHeader->packetLength, remainingSize);
				}
				currentPacketSize = remainingSize;
			}
			else
			{
				// 패킷이 아직 덜 옴
				break;
			}
		}
	}
}


/**
 * @brief (Main 스레드) 사용자 입력을 받아 서버로 전송
 */
int main()
{
	// 1. Winsock 초기화
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cerr << "WSAStartup failed!" << std::endl;
		return 1;
	}

	// 2. 클라이언트 소켓 생성
	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSocket == INVALID_SOCKET)
	{
		std::cerr << "Socket creation failed!" << std::endl;
		WSACleanup();
		return 1;
	}

	// 3. 서버 주소 설정
	sockaddr_in serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

	// 4. 서버에 연결 (Blocking)
	if (connect(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		std::cerr << "Connect failed! Server is not running." << std::endl;
		closesocket(serverSocket);
		WSACleanup();
		return 1;
	}

	std::cout << "서버에 성공적으로 접속했습니다." << std::endl;

	// 5. 로그인
	std::cout << "사용할 아이디를 입력하세요: ";
	std::string userID;
	std::getline(std::cin, userID);

	PktLoginReq loginReq;
	loginReq.packetLength = sizeof(loginReq);
	loginReq.type = PacketType::LoginReq;
	strncpy_s(loginReq.userID, userID.c_str(), MAX_USER_ID_LEN);

	if (send(serverSocket, (char*)&loginReq, loginReq.packetLength, 0) == SOCKET_ERROR)
	{
		std::cerr << "Login send failed!" << std::endl;
		return 1;
	}

	// 6. Recv 스레드 시작
	std::thread recvTh(RecvThread, serverSocket);
	recvTh.detach(); // 스레드를 분리

	// 7. (Main 스레드) Send 루프 시작
	// [랜덤 입장 추가] 도움말 수정
	std::cout << "--- 채팅 시작 (명령어: /create, /enter [ID], /leave, /random, 종료: /exit) ---" << std::endl;
	std::string input;
	while (true)
	{
		std::getline(std::cin, input);

		if (input.empty()) continue;
		if (input == "/exit") break;

		// (요구사항) 명령어 파싱
		if (input == "/create")
		{
			PktCreateRoomReq req;
			req.packetLength = sizeof(req);
			req.type = PacketType::CreateRoomReq;
			send(serverSocket, (char*)&req, req.packetLength, 0);
		}
		else if (input.rfind("/enter ", 0) == 0) // "/enter "로 시작하는지
		{
			try
			{
				int roomID = std::stoi(input.substr(7)); // "/enter " 다음의 숫자
				PktEnterRoomReq req;
				req.packetLength = sizeof(req);
				req.type = PacketType::EnterRoomReq;
				req.roomID = roomID;
				send(serverSocket, (char*)&req, req.packetLength, 0);
			}
			catch (...)
			{
				std::cout << "[System] 잘못된 명령어입니다. 예: /enter 0" << std::endl;
			}
		}
		else if (input == "/leave")
		{
			PktLeaveRoomReq req;
			req.packetLength = sizeof(req);
			req.type = PacketType::LeaveRoomReq;
			send(serverSocket, (char*)&req, req.packetLength, 0);
		}
		// [랜덤 입장 추가]
		else if (input == "/random")
		{
			PktEnterRandomRoomReq req;
			req.packetLength = sizeof(req);
			req.type = PacketType::EnterRandomRoomReq;
			send(serverSocket, (char*)&req, req.packetLength, 0);
		}
		else
		{
			// (요구사항) 일반 채팅
			PktChatReq req;
			req.packetLength = sizeof(req);
			req.type = PacketType::ChatReq;
			strncpy_s(req.message, input.c_str(), MAX_CHAT_LEN);
			send(serverSocket, (char*)&req, req.packetLength, 0);
		}
	}

	// 8. 종료
	closesocket(serverSocket);
	WSACleanup();

	return 0;
}