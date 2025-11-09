#pragma once

// --- 공통 상수 정의 ---
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
};

// S -> C : 로그인 응답
struct PktLoginRes : public PacketHeader
{
	bool success;
};

// C -> S : 채팅방 생성 요청
struct PktCreateRoomReq : public PacketHeader
{
};

// S -> C : 채팅방 생성 응답
struct PktCreateRoomRes : public PacketHeader
{
	bool success;
	int newRoomID;
};

// C -> S : 채팅방 입장 요청
struct PktEnterRoomReq : public PacketHeader
{
	int roomID;
};

// S -> C : 채팅방 입장 응답
struct PktEnterRoomRes : public PacketHeader
{
	bool success;
	int roomID;
};

// C -> S : 채팅방 퇴장 요청
struct PktLeaveRoomReq : public PacketHeader
{
};

// S -> C : 채팅방 퇴장 응답
struct PktLeaveRoomRes : public PacketHeader
{
	bool success;
};

// C -> S : 채팅 전송
struct PktChatReq : public PacketHeader
{
	char message[MAX_CHAT_LEN];
};

// S -> C : 채팅 알림 (브로드캐스팅용)
struct PktChatNtf : public PacketHeader
{
	char userID[MAX_USER_ID_LEN];
	char message[MAX_CHAT_LEN];
};

// S -> C : (로비/방) 새 유저 입장 알림
struct PktUserEnterNtf : public PacketHeader
{
	char userID[MAX_USER_ID_LEN];
};

// S -> C : (로비/방) 유저 퇴장 알림
struct PktUserLeaveNtf : public PacketHeader
{
	char userID[MAX_USER_ID_LEN];
};

// S -> C : (방/로비) 유저 리스트 알림 (클라이언트에만 있던 구조체 통합)
struct PktUserListNtf : public PacketHeader
{
	short userCount;
	// 뒤에 char[MAX_USER_ID_LEN] * userCount 만큼 데이터가 붙음
};

// [랜덤 입장 추가]
// C -> S : 랜덤 방 입장 요청
struct PktEnterRandomRoomReq : public PacketHeader
{
};


#pragma pack(pop)