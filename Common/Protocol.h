#pragma once

// --- 공통 상수 정의 ---
constexpr const char* SERVER_IP = "127.0.0.1"; // 서버 IP (localhost)
constexpr int SERVER_PORT = 9000;
constexpr int MAX_BUFFER_SIZE = 4096; // 공통 버퍼 크기
constexpr int MAX_ROOM_USERS = 50;
constexpr int LOBBY_ID = -1;
constexpr int MAX_USER_ID_LEN = 16;
constexpr int MAX_CHAT_LEN = 128;


// --- 패킷 프로토콜 정의 ---
// C/C++ 컴파일러가 구조체를 메모리에 정렬할 때
// 멤버 변수 사이에 패딩(빈 공간)을 넣지 않도록 1바이트 크기로 정렬
#pragma pack(push, 1)

// 패킷의 종류 (서버와 클라이언트의 모든 타입을 통합)
enum class PacketType : short
{
	// Client -> Server
	LoginReq,
	CreateRoomReq,
	EnterRoomReq,
	LeaveRoomReq,
	ChatReq,
	EnterRandomRoomReq, // [랜덤 입장 추가]

	// Server -> Client
	LoginRes,
	CreateRoomRes,
	EnterRoomRes,
	LeaveRoomRes,
	ChatNtf,
	UserEnterNtf,
	UserLeaveNtf,
	UserListNtf, // (클라이언트에만 있던 타입 통합)
};

// 모든 패킷의 기본이 되는 헤더
struct PacketHeader
{
	short packetLength;
	PacketType type;
};

// C -> S : 로그인 요청
struct PktLoginReq : public PacketHeader
{
	char userID[MAX_USER_ID_LEN];

	// [수정] 생성자 추가
	PktLoginReq()
	{
		memset(this, 0, sizeof(PktLoginReq));
		packetLength = sizeof(PktLoginReq);
		type = PacketType::LoginReq;
	}
};

// S -> C : 로그인 응답
struct PktLoginRes : public PacketHeader
{
	bool success;

	// [수정] 생성자 추가
	PktLoginRes()
	{
		memset(this, 0, sizeof(PktLoginRes));
		packetLength = sizeof(PktLoginRes);
		type = PacketType::LoginRes;
	}
};

// C -> S : 채팅방 생성 요청
struct PktCreateRoomReq : public PacketHeader
{
	// [수정] 생성자 추가
	PktCreateRoomReq()
	{
		memset(this, 0, sizeof(PktCreateRoomReq));
		packetLength = sizeof(PktCreateRoomReq);
		type = PacketType::CreateRoomReq;
	}
};

// S -> C : 채팅방 생성 응답
struct PktCreateRoomRes : public PacketHeader
{
	bool success;
	int newRoomID;

	// [수정] 생성자 추가
	PktCreateRoomRes()
	{
		memset(this, 0, sizeof(PktCreateRoomRes));
		packetLength = sizeof(PktCreateRoomRes);
		type = PacketType::CreateRoomRes;
	}
};

// C -> S : 채팅방 입장 요청
struct PktEnterRoomReq : public PacketHeader
{
	int roomID;

	// [수정] 생성자 추가
	PktEnterRoomReq()
	{
		memset(this, 0, sizeof(PktEnterRoomReq));
		packetLength = sizeof(PktEnterRoomReq);
		type = PacketType::EnterRoomReq;
	}
};

// S -> C : 채팅방 입장 응답
struct PktEnterRoomRes : public PacketHeader
{
	bool success;
	int roomID;

	// [수정] 생성자 추가
	PktEnterRoomRes()
	{
		memset(this, 0, sizeof(PktEnterRoomRes));
		packetLength = sizeof(PktEnterRoomRes);
		type = PacketType::EnterRoomRes;
	}
};

// C -> S : 채팅방 퇴장 요청
struct PktLeaveRoomReq : public PacketHeader
{
	// [수정] 생성자 추가
	PktLeaveRoomReq()
	{
		memset(this, 0, sizeof(PktLeaveRoomReq));
		packetLength = sizeof(PktLeaveRoomReq);
		type = PacketType::LeaveRoomReq;
	}
};

// S -> C : 채팅방 퇴장 응답
struct PktLeaveRoomRes : public PacketHeader
{
	bool success;

	// [수정] 생성자 추가
	PktLeaveRoomRes()
	{
		memset(this, 0, sizeof(PktLeaveRoomRes));
		packetLength = sizeof(PktLeaveRoomRes);
		type = PacketType::LeaveRoomRes;
	}
};

// C -> S : 채팅 전송
struct PktChatReq : public PacketHeader
{
	char message[MAX_CHAT_LEN];

	// [수정] 생성자 추가
	PktChatReq()
	{
		memset(this, 0, sizeof(PktChatReq));
		packetLength = sizeof(PktChatReq);
		type = PacketType::ChatReq;
	}
};

// S -> C : 채팅 알림 (브로드캐스팅용)
struct PktChatNtf : public PacketHeader
{
	char userID[MAX_USER_ID_LEN];
	char message[MAX_CHAT_LEN];

	// [수정] 생성자 추가
	PktChatNtf()
	{
		memset(this, 0, sizeof(PktChatNtf));
		packetLength = sizeof(PktChatNtf);
		type = PacketType::ChatNtf;
	}
};

// S -> C : (로비/방) 새 유저 입장 알림
struct PktUserEnterNtf : public PacketHeader
{
	char userID[MAX_USER_ID_LEN];

	// [수정] 생성자 추가
	PktUserEnterNtf()
	{
		memset(this, 0, sizeof(PktUserEnterNtf));
		packetLength = sizeof(PktUserEnterNtf);
		type = PacketType::UserEnterNtf;
	}
};

// S -> C : (로비/방) 유저 퇴장 알림
struct PktUserLeaveNtf : public PacketHeader
{
	char userID[MAX_USER_ID_LEN];

	// [수정] 생성자 추가
	PktUserLeaveNtf()
	{
		memset(this, 0, sizeof(PktUserLeaveNtf));
		packetLength = sizeof(PktUserLeaveNtf);
		type = PacketType::UserLeaveNtf;
	}
};

// S -> C : (방/로비) 유저 리스트 알림 (클라이언트에만 있던 구조체 통합)
struct PktUserListNtf : public PacketHeader
{
	short userCount;
	// 뒤에 char[MAX_USER_ID_LEN] * userCount 만큼 데이터가 붙음

	// [수정] 생성자 추가
	PktUserListNtf()
	{
		memset(this, 0, sizeof(PktUserListNtf));
		// 주의: 가변 길이 패킷이므로 packetLength는 실제 전송 시 덮어써야 함
		packetLength = sizeof(PktUserListNtf);
		type = PacketType::UserListNtf;
	}
};

// [랜덤 입장 추가]
// C -> S : 랜덤 방 입장 요청
struct PktEnterRandomRoomReq : public PacketHeader
{
	// [수정] 생성자 추가
	PktEnterRandomRoomReq()
	{
		memset(this, 0, sizeof(PktEnterRandomRoomReq));
		packetLength = sizeof(PktEnterRandomRoomReq);
		type = PacketType::EnterRandomRoomReq;
	}
};


#pragma pack(pop)