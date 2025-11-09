#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <string>		// (v2 추가)
#include <atomic>		// (v2 추가)
#include <memory>		// (v2 추가)
#include <random>		// (랜덤 입장 추가)
#include <chrono>		// [Timeout 추가]

#include "Protocol.h"

#pragma comment(lib, "ws2_32.lib")

// [Timeout 추가] --- 타임아웃 설정 ---
// 마지막 활동 후 60초가 지나면 타임아웃
constexpr int SESSION_TIMEOUT_SECONDS = 60;
// 5초마다 타임아웃 검사
constexpr int TIMEOUT_CHECK_INTERVAL_MS = 5000;


// I/O 작업의 종류를 구분하기 위한 열거형
enum class IOOperation
{
	Recv,
	Send
};

// OVERLAPPED 구조체를 확장
struct OverlappedEx
{
	OVERLAPPED overlapped;
	IOOperation operation;
	WSABUF wsaBuf;
	char buffer[MAX_BUFFER_SIZE];
};

// (v2 수정) 클라이언트 세션 구조체
struct Session
{
	SOCKET socket;
	OverlappedEx recvOverlapped;

	int currentRoomID;
	std::string userID;
	bool isLoggedIn;

	// (v2 추가) 패킷 조립을 위한 버퍼
	char packetBuffer[MAX_BUFFER_SIZE * 2];
	int currentPacketSize; // 현재까지 조립된 패킷 크기

	// [Timeout 추가] 마지막 활동 시간을 기록 (원자적 접근)
	std::atomic<std::chrono::steady_clock::time_point> lastActivityTime;

	Session(SOCKET s) :
		socket(s),
		currentRoomID(LOBBY_ID),
		isLoggedIn(false),
		currentPacketSize(0),
		lastActivityTime(std::chrono::steady_clock::now()) // [Timeout 추가]
	{
		ZeroMemory(&recvOverlapped, sizeof(OverlappedEx));
		recvOverlapped.operation = IOOperation::Recv;
		recvOverlapped.wsaBuf.buf = recvOverlapped.buffer;
		recvOverlapped.wsaBuf.len = MAX_BUFFER_SIZE;
		ZeroMemory(packetBuffer, sizeof(packetBuffer));
	}

	// (v2 추가) 세션 정리
	void Clear()
	{
		// TODO: 필요한 정리 작업
		isLoggedIn = false;
		currentRoomID = LOBBY_ID;
		userID = "";
	}
};

// (v2 추가) --- 전역 관리자 ---
class Room; // 전방 선언
class Lobby;

// 서버에서 사용하는 모든 Send 함수는 이 함수를 통하도록 함
void PostSend(Session* pSession, char* pPacket, int size);
void ProcessPacket(Session* pSession, char* pPacketData);

// (v2 추가) --- Room 클래스 ---
class Room
{
public:
	Room(int id) : m_roomID(id) {}

	// 유저 추가
	bool AddUser(Session* pSession)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_sessions.size() >= MAX_ROOM_USERS)
		{
			return false; // (요구사항) 방이 꽉 참
		}

		m_sessions[pSession->socket] = pSession;
		pSession->currentRoomID = m_roomID;

		std::cout << "[Room " << m_roomID << "] User '" << pSession->userID << "' entered. (Total: " << m_sessions.size() << ")" << std::endl;

		// (요구사항) 방에 있는 유저 식별
		// TODO: 새로 들어온 유저에게 현재 방의 유저 리스트 전송
		// TODO: 기존 방 유저들에게 새 유저 입장 알림 (PktUserEnterNtf)

		return true;
	}

	// 유저 제거
	void RemoveUser(Session* pSession)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_sessions.erase(pSession->socket);
		pSession->currentRoomID = LOBBY_ID; // 로비로 설정

		std::cout << "[Room " << m_roomID << "] User '" << pSession->userID << "' left. (Total: " << m_sessions.size() << ")" << std::endl;

		// TODO: 방에 남아있는 유저들에게 퇴장 알림 (PktUserLeaveNtf)
	}

	// (요구사항) 브로드캐스팅
	void Broadcast(char* pPacket, int size, SOCKET exceptSocket = INVALID_SOCKET)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		for (auto& pair : m_sessions)
		{
			if (pair.first != exceptSocket)
			{
				PostSend(pair.second, pPacket, size);
			}
		}
	}

	int GetID() { return m_roomID; }
	int GetUserCount() {
		std::lock_guard<std::mutex> lock(m_mutex);
		return static_cast<int>(m_sessions.size());
	}

private:
	std::mutex m_mutex; // (v2 추가) 이 방의 데이터 보호용 뮤텍스
	int m_roomID;
	std::unordered_map<SOCKET, Session*> m_sessions;
};

// (v2 추가) --- Lobby 클래스 ---
// Room과 거의 동일하지만, 입장 제한이 없음
class Lobby
{
public:
	void AddUser(Session* pSession)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_sessions[pSession->socket] = pSession;
		pSession->currentRoomID = LOBBY_ID;

		std::cout << "[Lobby] User '" << pSession->userID << "' entered. (Total: " << m_sessions.size() << ")" << std::endl;

		// (요구사항) 로비에 있는 유저 식별
		// TODO: 새로 들어온 유저에게 로비 유저 리스트 전송
		// TODO: 기존 로비 유저들에게 새 유저 입장 알림 (PktUserEnterNtf)
	}

	void RemoveUser(Session* pSession)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_sessions.erase(pSession->socket);

		std::cout << "[Lobby] User '" << pSession->userID << "' left. (Total: " << m_sessions.size() << ")" << std::endl;

		// TODO: 로비에 남아있는 유저들에게 퇴장 알림 (PktUserLeaveNtf)
	}

	void Broadcast(char* pPacket, int size, SOCKET exceptSocket = INVALID_SOCKET)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		for (auto& pair : m_sessions)
		{
			if (pair.first != exceptSocket)
			{
				PostSend(pair.second, pPacket, size);
			}
		}
	}

private:
	std::mutex m_mutex;
	std::unordered_map<SOCKET, Session*> m_sessions;
};


// (v2 추가) --- RoomManager 클래스 ---
// 방을 생성하고 관리
class RoomManager
{
public:
	Room* CreateRoom()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		int newRoomID = m_nextRoomID++;
		Room* pRoom = new Room(newRoomID);
		m_rooms[newRoomID] = pRoom;

		std::cout << "[System] Room " << newRoomID << " created." << std::endl;
		return pRoom;
	}

	Room* GetRoom(int roomID)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_rooms.find(roomID);
		if (it != m_rooms.end())
		{
			return it->second;
		}
		return nullptr;
	}

	void RemoveRoom(int roomID)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_rooms.find(roomID);
		if (it != m_rooms.end())
		{
			delete it->second;
			m_rooms.erase(it);
			std::cout << "[System] Room " << roomID << " removed." << std::endl;
		}
	}

	// [랜덤 입장 추가]
	/**
	 * @brief 입장 가능한 (꽉 차지 않은) 방을 랜덤하게 찾습니다.
	 * @return 입장 가능한 Room 포인터. 없으면 nullptr.
	 */
	Room* GetRandomAvailableRoomOrNull()
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		std::vector<Room*> availableRooms;
		availableRooms.reserve(m_rooms.size()); // 최적화를 위해 미리 공간 확보

		// 1. 꽉 차지 않은 방 리스트 수집
		for (auto& pair : m_rooms)
		{
			if (pair.second->GetUserCount() < MAX_ROOM_USERS)
			{
				availableRooms.push_back(pair.second);
			}
		}

		// 2. 입장 가능한 방이 없으면 nullptr 반환
		if (availableRooms.empty())
		{
			return nullptr;
		}

		// 3. C++11 스타일 랜덤 생성
		// static: 스레드마다 고유하지 않고 이 함수에서 공유됨 (성능상 무방)
		static std::random_device rd;
		static std::mt19937 gen(rd()); // 난수 엔진 (Mersenne Twister)

		// 0 ~ (size-1) 사이의 균일 분포 생성
		std::uniform_int_distribution<> dis(0, static_cast<int>(availableRooms.size() - 1));

		// 4. 랜덤 방 반환
		return availableRooms[dis(gen)];
	}


private:
	std::mutex m_mutex;
	std::unordered_map<int, Room*> m_rooms;
	std::atomic<int> m_nextRoomID = 0; // 방 ID는 0부터 시작
};


// --- 전역 변수 ---
HANDLE g_iocpHandle;
Lobby g_Lobby;			// (v2 추가)
RoomManager g_RoomManager; // (v2 추가)

// [Timeout 추가]
// 모든 활성 세션을 관리하기 위한 전역 맵 및 뮤텍스
std::unordered_map<SOCKET, Session*> g_sessions;
std::mutex g_sessionMutex;


// (v2 추가) --- 핵심 로직 함수 ---

/**
 * @brief 비동기 Send 요청
 * @details 이 함수는 언제나 성공한다고 가정하고 Send용 OverlappedEx를 'new'로 할당.
 * Send 작업이 완료되면 WorkerThread의 'IOOperation::Send'에서 'delete' 해줘야 함.
 */
void PostSend(Session* pSession, char* pPacket, int size)
{
	OverlappedEx* pOverlappedEx = new OverlappedEx();
	ZeroMemory(pOverlappedEx, sizeof(OverlappedEx));

	pOverlappedEx->operation = IOOperation::Send;
	memcpy(pOverlappedEx->buffer, pPacket, size); // 보낼 데이터를 복사
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
		// 실패 시에도 Send 완료 통지는 오지 않으므로 여기서 delete
		delete pOverlappedEx;
	}
}

/**
 * @brief (v2 추가) 완성된 패킷을 처리하는 함수
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

		// (요구사항) 로비 입장
		g_Lobby.AddUser(pSession);

		// 응답 전송
		PktLoginRes res;
		res.packetLength = sizeof(res);
		res.type = PacketType::LoginRes;
		res.success = true;
		PostSend(pSession, (char*)&res, res.packetLength);
		break;
	}

	case PacketType::CreateRoomReq:
	{
		if (!pSession->isLoggedIn || pSession->currentRoomID != LOBBY_ID) break; // 로비에 없으면 방 생성 불가

		// 1. 기존 로비에서 나감
		g_Lobby.RemoveUser(pSession);

		// 2. 새 방 생성
		Room* pNewRoom = g_RoomManager.CreateRoom();

		// 3. (요구사항) 방 입장
		pNewRoom->AddUser(pSession); // (방장은 항상 입장 성공)

		// 4. 응답 전송
		PktCreateRoomRes res;
		res.packetLength = sizeof(res);
		res.type = PacketType::CreateRoomRes;
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

		PktEnterRoomRes res; // 응답 패킷 미리 준비
		res.packetLength = sizeof(res);
		res.type = PacketType::EnterRoomRes;

		if (pRoom == nullptr) // 방이 없음
		{
			res.success = false;
		}
		else
		{
			// (요구사항) 50명 제한
			if (pRoom->AddUser(pSession))
			{
				// 입장 성공
				g_Lobby.RemoveUser(pSession); // 로비에서 제거
				res.success = true;
				res.roomID = pRoom->GetID();
			}
			else
			{
				// 입장 실패 (방 꽉 참)
				res.success = false;
			}
		}
		PostSend(pSession, (char*)&res, res.packetLength);
		break;
	}

	// [랜덤 입장 추가]
	case PacketType::EnterRandomRoomReq:
	{
		if (!pSession->isLoggedIn || pSession->currentRoomID != LOBBY_ID) break;

		// 1. 입장 가능한 랜덤 방 탐색
		Room* pRoom = g_RoomManager.GetRandomAvailableRoomOrNull();

		PktEnterRoomRes res; // 응답은 EnterRoomRes와 동일
		res.packetLength = sizeof(res);
		res.type = PacketType::EnterRoomRes;

		if (pRoom == nullptr) // 입장 가능한 방이 없음
		{
			std::cout << "[System] No available rooms for random entry for User '" << pSession->userID << "'." << std::endl;
			res.success = false;
		}
		else
		{
			// 2. 방 입장 시도 (방이 꽉 찼는지 다시 확인)
			if (pRoom->AddUser(pSession))
			{
				// 입장 성공
				g_Lobby.RemoveUser(pSession); // 로비에서 제거
				res.success = true;
				res.roomID = pRoom->GetID();
			}
			else
			{
				// 입장 실패 (그 사이에 방이 꽉 참 - Race Condition)
				std::cout << "[System] Random entry failed (Race Condition) for User '" << pSession->userID << "'." << std::endl;
				res.success = false;
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
			pRoom->RemoveUser(pSession); // 방에서 제거
			if (pRoom->GetUserCount() == 0)
			{
				// (v2 추가) 방이 비었으면 제거
				g_RoomManager.RemoveRoom(pRoom->GetID());
			}
		}

		g_Lobby.AddUser(pSession); // 로비로 이동

		// 응답 전송
		PktLeaveRoomRes res;
		res.packetLength = sizeof(res);
		res.type = PacketType::LeaveRoomRes;
		res.success = true;
		PostSend(pSession, (char*)&res, res.packetLength);
		break;
	}

	case PacketType::ChatReq:
	{
		if (!pSession->isLoggedIn) break;

		PktChatReq* pReq = reinterpret_cast<PktChatReq*>(pPacketData);

		// (요구사항) 브로드캐스팅
		PktChatNtf ntf; // 알림 패킷 생성
		ntf.packetLength = sizeof(ntf);
		ntf.type = PacketType::ChatNtf;
		strncpy_s(ntf.userID, pSession->userID.c_str(), MAX_USER_ID_LEN);
		strncpy_s(ntf.message, pReq->message, MAX_CHAT_LEN);

		if (pSession->currentRoomID == LOBBY_ID)
		{
			// 로비 채팅
			g_Lobby.Broadcast((char*)&ntf, ntf.packetLength);
		}
		else
		{
			// 방 채팅
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
 * @brief (v2 추가) 수신된 데이터를 파싱하고 패킷을 조립/처리하는 함수
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
		// 1. 헤더 크기만큼은 데이터가 있는지 확인
		PacketHeader* pHeader = reinterpret_cast<PacketHeader*>(pSession->packetBuffer);

		// 2. 패킷 전체 크기(length)만큼 데이터가 모두 도착했는지 확인
		if (pSession->currentPacketSize >= pHeader->packetLength)
		{
			// 3. 패킷이 완성됨 -> 처리
			ProcessPacket(pSession, pSession->packetBuffer);

			// 4. 처리한 패킷 크기만큼 버퍼에서 제거 (뒤쪽 데이터를 앞으로 당김)
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
			// 5. 헤더는 왔지만 데이터가 아직 덜 옴 (TCP 분할 수신)
			//   -> 다음 Recv를 기다림
			break;
		}
	}
}


// (v2 수정) --- 워커 스레드 함수 ---
void WorkerThread()
{
	DWORD bytesTransferred;
	ULONG_PTR completionKey;
	LPOVERLAPPED lpOverlapped;

	std::cout << "[Debug] Worker Thread " << std::this_thread::get_id() << " started." << std::endl;

	while (true)
	{
		bool result = GetQueuedCompletionStatus(
			g_iocpHandle,
			&bytesTransferred,
			&completionKey,
			&lpOverlapped,
			INFINITE
		);

		Session* pSession = reinterpret_cast<Session*>(completionKey);
		if (pSession == nullptr)
		{
			// (예: 서버 종료 시 PostQueuedCompletionStatus(g_iocpHandle, 0, NULL, NULL))
			std::cerr << "Worker thread " << std::this_thread::get_id() << " exiting..." << std::endl;
			break;
		}

		OverlappedEx* pOverlappedEx = reinterpret_cast<OverlappedEx*>(lpOverlapped);

		// (v2 수정) 클라이언트 접속 종료 처리
		if (!result || bytesTransferred == 0)
		{
			// [Timeout 추가]
			// !result: 소켓 에러 (TimeoutThread가 closesocket() 호출 시 여기로 들어옴)
			// bytesTransferred == 0: 클라이언트가 정상 종료
			if (!result)
			{
				std::cout << "Client disconnected (Socket Error " << WSAGetLastError() << ", User: " << pSession->userID << ")" << std::endl;
			}
			else
			{
				std::cout << "Client disconnected (Graceful, User: " << pSession->userID << ")" << std::endl;
			}


			// (v2 추가) 유저가 있던 곳(로비/방)에서 제거
			if (pSession->isLoggedIn)
			{
				if (pSession->currentRoomID == LOBBY_ID)
				{
					g_Lobby.RemoveUser(pSession);
				}
				else
				{
					Room* pRoom = g_RoomManager.GetRoom(pSession->currentRoomID);
					if (pRoom)
					{
						pRoom->RemoveUser(pSession);
						if (pRoom->GetUserCount() == 0)
						{
							g_RoomManager.RemoveRoom(pRoom->GetID());
						}
					}
				}
			}

			// [Timeout 추가] 전역 세션 맵에서 제거
			{
				std::lock_guard<std::mutex> lock(g_sessionMutex);
				g_sessions.erase(pSession->socket);
			}

			closesocket(pSession->socket);
			delete pSession; // 세션 객체 삭제
			continue;
		}

		// 완료된 I/O 작업 처리
		switch (pOverlappedEx->operation)
		{
		case IOOperation::Recv:
		{
			// [Timeout 추가]
			// 클라이언트로부터 데이터를 받았으므로 마지막 활동 시간 갱신
			pSession->lastActivityTime = std::chrono::steady_clock::now();

			// (v2 수정) 패킷 처리 로직을 별도 함수로 분리
			ProcessRecv(pSession, bytesTransferred);

			// (중요) 다음 비동기 수신(WSARecv)을 다시 요청
			DWORD recvBytes = 0;
			DWORD flags = 0;
			// 세션의 Recv용 OverlappedEx를 재사용
			ZeroMemory(&(pSession->recvOverlapped.overlapped), sizeof(OVERLAPPED));
			pSession->recvOverlapped.wsaBuf.len = MAX_BUFFER_SIZE; // 버퍼 크기 재설정

			int recvResult = WSARecv(
				pSession->socket,
				&(pSession->recvOverlapped.wsaBuf),
				1,
				&recvBytes,
				&flags,
				&(pSession->recvOverlapped.overlapped),
				NULL
			);

			if (recvResult == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
			{
				std::cerr << "WSARecv failed after processing! Error: " << WSAGetLastError() << std::endl;
				// TODO: 이 경우에도 연결 종료 처리가 필요
			}
			break;
		}

		case IOOperation::Send:
		{
			// (v2 수정) Send 작업에 사용된 OverlappedEx는 동적 할당되었으므로 해제
			delete pOverlappedEx;
			break;
		}
		}
	}
}

// [Timeout 추가]
/**
 * @brief 주기적으로 모든 세션을 검사하여 타임아웃된 세션을 정리하는 스레드
 */
void TimeoutThread()
{
	std::cout << "[Debug] Timeout Thread " << std::this_thread::get_id() << " started." << std::endl;

	while (true)
	{
		// 1. 일정 시간 대기
		std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT_CHECK_INTERVAL_MS));

		auto now = std::chrono::steady_clock::now();

		// 강제 종료할 소켓 리스트 (g_sessionMutex 락을 오래 잡지 않기 위함)
		std::vector<SOCKET> timedOutSockets;

		// 2. 전체 세션 맵(g_sessions)을 잠그고 순회
		{
			std::lock_guard<std::mutex> lock(g_sessionMutex);

			if (g_sessions.empty()) continue; // (최적화)

			// g_sessions를 순회하면서 타임아웃된 세션을 찾음
			for (auto& pair : g_sessions)
			{
				Session* pSession = pair.second;

				// (중요) atomic<time_point> 읽기
				auto lastActivity = pSession->lastActivityTime.load();

				auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - lastActivity).count();

				if (elapsedSeconds > SESSION_TIMEOUT_SECONDS)
				{
					// 타임아웃
					timedOutSockets.push_back(pSession->socket);
				}
			}
		} // (Mutex Unlock)

		// 3. (중요) 맵 락을 푼 상태에서 소켓을 닫음
		// 맵을 잠근 상태로 closesocket을 호출하면 데드락 위험이 있음
		// (WorkerThread가 closesocket으로 인해 GQCS에서 깨어나 g_sessionMutex를 잡으려 할 수 있음)
		if (!timedOutSockets.empty())
		{
			std::cout << "[Timeout] Disconnecting " << timedOutSockets.size() << " inactive clients..." << std::endl;
			for (SOCKET sock : timedOutSockets)
			{
				// closesocket()을 호출하면 해당 소켓의 모든 보류 중인
				// I/O 작업(WSARecv)이 즉시 실패하고,
				// WorkerThread의 GetQueuedCompletionStatus가
				// 'result == false'로 리턴됨.
				// 그러면 WorkerThread가 기존 연결 종료 로직(g_sessions에서 제거 등)을 수행함.
				closesocket(sock);
			}
		}
	}
}


// --- 메인 스레드 함수 (v1과 거의 동일) ---
int main()
{
	// 1. Winsock 초기화
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cerr << "WSAStartup failed!" << std::endl;
		return 1;
	}

	// 2. IOCP 포트 생성
	g_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (g_iocpHandle == NULL)
	{
		std::cerr << "CreateIoCompletionPort failed!" << std::endl;
		WSACleanup();
		return 1;
	}

	// 3. 워커 스레드 생성
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	int threadCount = sysInfo.dwNumberOfProcessors;
	std::vector<std::thread> workerThreads;

	for (int i = 0; i < threadCount; ++i)
	{
		workerThreads.emplace_back(WorkerThread);
	}
	std::cout << "Server :: " << threadCount << " worker threads created." << std::endl;

	// [Timeout 추가] 타임아웃 스레드 시작
	std::thread timeoutTh(TimeoutThread);
	timeoutTh.detach(); // 메인 스레드와 분리


	// 4. 리슨 소켓 생성
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSocket == INVALID_SOCKET)
	{
		std::cerr << "Listen socket creation failed!" << std::endl;
		WSACleanup();
		return 1;
	}

	// 5. 서버 주소 설정 및 바인딩
	sockaddr_in serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(SERVER_PORT);

	if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		std::cerr << "Socket bind failed!" << std::endl;
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}

	// 6. 리슨 시작
	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		std::cerr << "Socket listen failed!" << std::endl;
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}

	std::cout << "Server :: Chat server listening on port " << SERVER_PORT << "..." << std::endl;

	// 7. 메인 스레드의 Accept 루프
	while (true)
	{
		sockaddr_in clientAddr;
		int addrLen = sizeof(clientAddr);
		SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &addrLen);

		if (clientSocket == INVALID_SOCKET)
		{
			std::cerr << "Accept failed!" << std::endl;
			continue;
		}

		char clientIp[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, INET_ADDRSTRLEN);
		std::cout << "Client connected from " << clientIp << ":" << ntohs(clientAddr.sin_port) << std::endl;

		// 1. (v2 수정) 세션 생성 (소켓 전달)
		Session* pSession = new Session(clientSocket);

		// [Timeout 추가] 전역 세션 맵에 추가
		{
			std::lock_guard<std::mutex> lock(g_sessionMutex);
			g_sessions[clientSocket] = pSession;
		}

		// 2. IOCP에 연결 (CompletionKey로 pSession 전달)
		CreateIoCompletionPort((HANDLE)clientSocket, g_iocpHandle, (ULONG_PTR)pSession, 0);

		// 3. 첫 번째 비동기 수신(WSARecv) 요청
		DWORD recvBytes = 0;
		DWORD flags = 0;
		int recvResult = WSARecv(
			pSession->socket,
			&(pSession->recvOverlapped.wsaBuf),
			1,
			&recvBytes,
			&flags,
			&(pSession->recvOverlapped.overlapped),
			NULL
		);

		if (recvResult == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
		{
			std::cerr << "WSARecv failed immediately!" << std::endl;

			// [Timeout 추가] 실패 시 전역 맵에서 즉시 제거
			{
				std::lock_guard<std::mutex> lock(g_sessionMutex);
				g_sessions.erase(clientSocket);
			}
			closesocket(clientSocket);
			delete pSession;
		}
	}

	// 8. 정리 (실제로는 거의 도달하지 않음)
	for (auto& th : workerThreads)
	{
		if (th.joinable())
		{
			// (실제로는 스레드를 종료시키기 위해
			//  PostQueuedCompletionStatus(g_iocpHandle, 0, NULL, NULL); 를 스레드 개수만큼 호출해야 함)
			th.join();
		}
	}
	closesocket(listenSocket);
	CloseHandle(g_iocpHandle);
	WSACleanup();
	return 0;
}