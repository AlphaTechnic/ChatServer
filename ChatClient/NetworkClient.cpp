#include "pch.h"
#include "NetworkClient.h"
#include "PacketHandler.h" // RecvThread가 ProcessPacket을 호출하기 위해

namespace NetworkClient
{
	// 이 cpp 파일 내에서만 사용되는 전역 소켓 및 상태
	static SOCKET g_serverSocket = INVALID_SOCKET;
	static std::atomic<bool> g_isConnected = false;

	// --- 내부 스레드 함수 ---

	/**
	 * @brief (Recv 스레드) 서버로부터 패킷을 수신하는 스레드
	 */
	static void RecvThread()
	{
		char recvBuffer[MAX_BUFFER_SIZE];
		char packetBuffer[MAX_BUFFER_SIZE * 2] = { 0 , };
		int currentPacketSize = 0;

		std::cout << "[Debug] Recv thread started." << std::endl;

		while (g_isConnected)
		{
			// 1. 데이터 수신 (Blocking)
			int nRecv = recv(g_serverSocket, recvBuffer, MAX_BUFFER_SIZE, 0);
			if (nRecv <= 0)
			{
				// 0: 서버가 정상 종료, -1: 소켓 오류 (Disconnect()에서 closesocket() 호출 시)
				if (g_isConnected) // Disconnect()가 아닌, 서버에 의해 끊겼을 때만 메시지 출력
				{
					std::cout << "[System] 서버와 연결이 끊어졌습니다." << std::endl;
				}
				g_isConnected = false; // 연결 상태 변경
				break;
			}

			// 2. 수신한 데이터를 패킷 조립 버퍼에 복사
			memcpy(packetBuffer + currentPacketSize, recvBuffer, nRecv);
			currentPacketSize += nRecv;

			// 3. 패킷 조립
			while (currentPacketSize >= sizeof(PacketHeader))
			{
				PacketHeader* pHeader = reinterpret_cast<PacketHeader*>(packetBuffer);
				if (currentPacketSize >= pHeader->packetLength)
				{
					// 패킷 완성 -> PacketHandler에 처리 위임
					PacketHandler::ProcessPacket(packetBuffer);

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
		std::cout << "[Debug] Recv thread stopped." << std::endl;
	}


	// --- 공개 함수 구현 ---

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
		g_isConnected = false; // Recv 스레드가 루프를 종료하도록 함
		if (g_serverSocket != INVALID_SOCKET)
		{
			// recv()에서 블로킹 중인 스레드를 깨우기 위해 소켓을 닫음
			closesocket(g_serverSocket);
			g_serverSocket = INVALID_SOCKET;
		}
	}

	void SendPacket(char* pPacket, int size)
	{
		if (!g_isConnected)
		{
			std::cout << "[System] 서버와 연결되어 있지 않습니다." << std::endl;
			return;
		}

		if (send(g_serverSocket, pPacket, size, 0) == SOCKET_ERROR)
		{
			std::cerr << "Send failed!" << std::endl;
			// (Send 실패 시 Disconnect()를 호출하여 정리하는 로직을 추가할 수 있음)
		}
	}

	void StartRecvThread()
	{
		// 스레드를 생성하고 즉시 분리(detach)
		std::thread(RecvThread).detach();
	}

	bool IsConnected()
	{
		return g_isConnected;
	}

} // namespace NetworkClient