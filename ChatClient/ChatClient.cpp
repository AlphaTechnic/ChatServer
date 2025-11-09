#include "pch.h"
#include "NetworkClient.h" // 우리가 만든 네트워크 모듈

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

	// 2. 서버에 연결 (NetworkClient 네임스페이스 함수 호출)
	if (!NetworkClient::ConnectToServer(SERVER_IP, SERVER_PORT))
	{
		WSACleanup();
		return 1;
	}

	std::cout << "서버에 성공적으로 접속했습니다." << std::endl;

	// 3. 로그인 (UI 로직이므로 main에 둠)
	std::cout << "사용할 아이디를 입력하세요: ";
	std::string userID;
	std::getline(std::cin, userID);

	PktLoginReq loginReq;
	strncpy_s(loginReq.userID, userID.c_str(), MAX_USER_ID_LEN);
	NetworkClient::SendPacket((char*)&loginReq, loginReq.packetLength);

	// 4. Recv 스레드 시작
	NetworkClient::StartRecvThread();

	// 5. (Main 스레드) Send/Input 루프 시작
	std::cout << "--- 채팅 시작 (명령어: /create, /enter [ID], /leave, /random, 종료: /exit) ---" << std::endl;
	std::string input;

	// NetworkClient::IsConnected()가 false가 되면 루프 종료
	// (Recv 스레드에서 연결이 끊기면 false가 됨)
	while (NetworkClient::IsConnected())
	{
		std::getline(std::cin, input);

		if (!NetworkClient::IsConnected()) break; // 입력 대기 중에 연결이 끊겼을 수 있음
		if (input.empty()) continue;
		if (input == "/exit") break;

		// 명령어 파싱
		if (input == "/create")
		{
			PktCreateRoomReq req;
			NetworkClient::SendPacket((char*)&req, req.packetLength);
		}
		else if (input.rfind("/enter ", 0) == 0) // "/enter "로 시작하는지
		{
			try
			{
				int roomID = std::stoi(input.substr(7)); // "/enter " 다음의 숫자
				PktEnterRoomReq req;
				req.roomID = roomID;
				NetworkClient::SendPacket((char*)&req, req.packetLength);
			}
			catch (...)
			{
				std::cout << "[System] 잘못된 명령어입니다. 예: /enter 0" << std::endl;
			}
		}
		else if (input == "/leave")
		{
			PktLeaveRoomReq req;
			NetworkClient::SendPacket((char*)&req, req.packetLength);
		}
		else if (input == "/random")
		{
			PktEnterRandomRoomReq req;
			NetworkClient::SendPacket((char*)&req, req.packetLength);
		}
		else
		{
			// 일반 채팅
			PktChatReq req;
			strncpy_s(req.message, input.c_str(), MAX_CHAT_LEN);
			NetworkClient::SendPacket((char*)&req, req.packetLength);
		}
	}

	// 6. 종료
	std::cout << "클라이언트를 종료합니다." << std::endl;
	NetworkClient::Disconnect();
	WSACleanup();

	return 0;
}
