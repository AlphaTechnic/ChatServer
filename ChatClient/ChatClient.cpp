#include "pch.h"
#include "NetworkClient.h"

int main()
{
    // initialize Winsock
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cerr << "WSAStartup failed!" << std::endl;
		return 1;
	}

    // connect to server
	if (!NetworkClient::ConnectToServer(SERVER_IP, SERVER_PORT))
	{
		WSACleanup();
		return 1;
	}

	std::cout << "서버에 성공적으로 접속했습니다." << std::endl;

	// login
	std::cout << "사용할 아이디를 입력하세요: ";
	std::string userID;
	std::getline(std::cin, userID);

	PktLoginReq loginReq;
	strncpy_s(loginReq.userID, userID.c_str(), MAX_USER_ID_LEN);
	NetworkClient::SendPacket((char*)&loginReq, loginReq.packetLength);

    // start receive thread
	NetworkClient::StartRecvThread();

	std::cout << "--- 채팅 시작 (명령어: /create, /enter [ID], /leave, /random, 종료: /exit) ---" << std::endl;
	std::string input;

    // it becomes false when the connection is lost in the Recv thread
	while (NetworkClient::IsConnected())
	{
		std::getline(std::cin, input);

        // while waiting for input, the connection may be lost
		if (!NetworkClient::IsConnected()) break;
		if (input.empty()) continue;

        // parse commands
		if (input == "/exit") break;
		if (input == "/create")
		{
			PktCreateRoomReq req;
			NetworkClient::SendPacket((char*)&req, req.packetLength);
		}
		else if (input.rfind("/enter ", 0) == 0)
		{
			try
            {
				int roomID = std::stoi(input.substr(7)); // extract room ID from command
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
			PktChatReq req;
			strncpy_s(req.message, input.c_str(), MAX_CHAT_LEN);
			NetworkClient::SendPacket((char*)&req, req.packetLength);
		}
	}

	std::cout << "클라이언트를 종료합니다." << std::endl;
	NetworkClient::Disconnect();
	WSACleanup();

	return 0;
}
