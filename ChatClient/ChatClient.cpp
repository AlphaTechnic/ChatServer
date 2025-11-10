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

    std::cout << "Successfully connected to the server." << std::endl;

	// login
    std::cout << "Enter your user ID: ";
	std::string userID;
	std::getline(std::cin, userID);

	PktLoginReq loginReq;
	strncpy_s(loginReq.userID, userID.c_str(), MAX_USER_ID_LEN);
	NetworkClient::SendPacket((char*)&loginReq, loginReq.packetLength);

    // start receive thread
	NetworkClient::StartRecvThread();

    std::cout << "--- Chat started (commands: /create, /enter [ID], /leave, /random, exit: /exit) ---" << std::endl;
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
                std::cout << "[System] Invalid command. Example: /enter 0" << std::endl;
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

    std::cout << "Exiting the chat client." << std::endl;
	NetworkClient::Disconnect();
	WSACleanup();

	return 0;
}
