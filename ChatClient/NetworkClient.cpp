#include "pch.h"
#include "NetworkClient.h"
#include "PacketHandler.h"

namespace NetworkClient
{
	static SOCKET g_serverSocket = INVALID_SOCKET;
	static std::atomic<bool> g_isConnected = false;

	// receiving packets from server
	static void RecvThread()
	{
		char recvBuffer[MAX_BUFFER_SIZE];
		char packetBuffer[MAX_BUFFER_SIZE * 2] = { 0 , };
		int currentPacketSize = 0;

		std::cout << "[Debug] Recv thread started." << std::endl;

		while (g_isConnected)
		{
            // receive data from server
			int nRecv = recv(g_serverSocket, recvBuffer, MAX_BUFFER_SIZE, 0);
            // 0: Server closed the connection gracefully
            // -1: Socket error when closesocket() is called in Disconnect()
			if (nRecv <= 0)
			{
				if (g_isConnected)
				{
                    std::cout << "[System] Disconnected from server." << std::endl;
				}
                g_isConnected = false;
				break;
			}

            // copy received data to packet assembly buffer
			memcpy(packetBuffer + currentPacketSize, recvBuffer, nRecv);
			currentPacketSize += nRecv;
			while (currentPacketSize >= sizeof(PacketHeader))
			{
				PacketHeader* pHeader = reinterpret_cast<PacketHeader*>(packetBuffer);
				if (currentPacketSize >= pHeader->packetLength)
				{
                    PacketHandler::ProcessPacket(packetBuffer);
					int remainingSize = currentPacketSize - pHeader->packetLength;
					if (remainingSize > 0)
					{
						memmove(packetBuffer, packetBuffer + pHeader->packetLength, remainingSize);
					}
					currentPacketSize = remainingSize;
				}
				else
				{
                    // packet is not fully received yet
					break;
				}
			}
		}
		std::cout << "[Debug] Recv thread stopped." << std::endl;
	}

	bool ConnectToServer(const char* serverIP, short serverPort)
	{
		g_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (g_serverSocket == INVALID_SOCKET)
		{
			std::cerr << "Socket creation failed!" << std::endl;
			return false;
		}

		sockaddr_in serverAddr;
		ZeroMemory(&serverAddr, sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(serverPort);
		inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);

		if (connect(g_serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
		{
			std::cerr << "Connect failed! Server is not running." << std::endl;
			closesocket(g_serverSocket);
			g_serverSocket = INVALID_SOCKET;
			return false;
		}

		g_isConnected = true;
		return true;
	}

	void Disconnect()
	{
        // set connection flag to false to stop Recv thread loop
		g_isConnected = false;
		if (g_serverSocket != INVALID_SOCKET)
		{
            // close the socket to unblock recv() in the recv thread
			closesocket(g_serverSocket);
			g_serverSocket = INVALID_SOCKET;
		}
	}

	void SendPacket(char* pPacket, int size)
	{
		if (!g_isConnected)
		{
            std::cout << "[System] Not connected to the server." << std::endl;
			return;
		}

		if (send(g_serverSocket, pPacket, size, 0) == SOCKET_ERROR)
		{
			std::cerr << "Send failed!" << std::endl;
		}
	}

	void StartRecvThread()
	{
		std::thread(RecvThread).detach();
	}

	bool IsConnected()
	{
		return g_isConnected;
	}

}
