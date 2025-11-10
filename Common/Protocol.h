#pragma once

constexpr const char* SERVER_IP = "127.0.0.1";
constexpr int SERVER_PORT = 9000;
constexpr int MAX_BUFFER_SIZE = 4096;
constexpr int MAX_ROOM_USERS = 50;
constexpr int LOBBY_ID = -1;
constexpr int MAX_USER_ID_LEN = 16;
constexpr int MAX_CHAT_LEN = 128;

// this directive tells the C/C++ compiler to pack structure members with 1-byte alignment, preventing any padding between members.
#pragma pack(push, 1)

enum class PacketType : short
{
	// Client -> Server
	LoginReq,
	CreateRoomReq,
	EnterRoomReq,
	LeaveRoomReq,
	ChatReq,
	EnterRandomRoomReq,

	// Server -> Client
	LoginRes,
	CreateRoomRes,
	EnterRoomRes,
	LeaveRoomRes,
	ChatNtf,
	UserEnterNtf,
	UserLeaveNtf,
	UserListNtf,
};

struct PacketHeader
{
	short packetLength;
	PacketType type;
};

struct PktLoginReq : public PacketHeader
{
	char userID[MAX_USER_ID_LEN];

	PktLoginReq()
	{
		memset(this, 0, sizeof(PktLoginReq));
		packetLength = sizeof(PktLoginReq);
		type = PacketType::LoginReq;
	}
};

struct PktLoginRes : public PacketHeader
{
	bool success;

	PktLoginRes()
	{
		memset(this, 0, sizeof(PktLoginRes));
		packetLength = sizeof(PktLoginRes);
		type = PacketType::LoginRes;
	}
};

struct PktCreateRoomReq : public PacketHeader
{
	PktCreateRoomReq()
	{
		memset(this, 0, sizeof(PktCreateRoomReq));
		packetLength = sizeof(PktCreateRoomReq);
		type = PacketType::CreateRoomReq;
	}
};

struct PktCreateRoomRes : public PacketHeader
{
	bool success;
	int newRoomID;

	PktCreateRoomRes()
	{
		memset(this, 0, sizeof(PktCreateRoomRes));
		packetLength = sizeof(PktCreateRoomRes);
		type = PacketType::CreateRoomRes;
	}
};

struct PktEnterRoomReq : public PacketHeader
{
	int roomID;

	PktEnterRoomReq()
	{
		memset(this, 0, sizeof(PktEnterRoomReq));
		packetLength = sizeof(PktEnterRoomReq);
		type = PacketType::EnterRoomReq;
	}
};

struct PktEnterRoomRes : public PacketHeader
{
	bool success;
	int roomID;

	PktEnterRoomRes()
	{
		memset(this, 0, sizeof(PktEnterRoomRes));
		packetLength = sizeof(PktEnterRoomRes);
		type = PacketType::EnterRoomRes;
	}
};

struct PktLeaveRoomReq : public PacketHeader
{
	PktLeaveRoomReq()
	{
		memset(this, 0, sizeof(PktLeaveRoomReq));
		packetLength = sizeof(PktLeaveRoomReq);
		type = PacketType::LeaveRoomReq;
	}
};

struct PktLeaveRoomRes : public PacketHeader
{
	bool success;

	PktLeaveRoomRes()
	{
		memset(this, 0, sizeof(PktLeaveRoomRes));
		packetLength = sizeof(PktLeaveRoomRes);
		type = PacketType::LeaveRoomRes;
	}
};

struct PktChatReq : public PacketHeader
{
	char message[MAX_CHAT_LEN];

	PktChatReq()
	{
		memset(this, 0, sizeof(PktChatReq));
		packetLength = sizeof(PktChatReq);
		type = PacketType::ChatReq;
	}
};

// S -> C : notify chat message
struct PktChatNtf : public PacketHeader
{
	char userID[MAX_USER_ID_LEN];
	char message[MAX_CHAT_LEN];

	PktChatNtf()
	{
		memset(this, 0, sizeof(PktChatNtf));
		packetLength = sizeof(PktChatNtf);
		type = PacketType::ChatNtf;
	}
};

// S -> C : notify new user entered (lobby/room)
struct PktUserEnterNtf : public PacketHeader
{
	char userID[MAX_USER_ID_LEN];

	PktUserEnterNtf()
	{
		memset(this, 0, sizeof(PktUserEnterNtf));
		packetLength = sizeof(PktUserEnterNtf);
		type = PacketType::UserEnterNtf;
	}
};

// S -> C : notify user left (lobby/room)
struct PktUserLeaveNtf : public PacketHeader
{
	char userID[MAX_USER_ID_LEN];

	PktUserLeaveNtf()
	{
		memset(this, 0, sizeof(PktUserLeaveNtf));
		packetLength = sizeof(PktUserLeaveNtf);
		type = PacketType::UserLeaveNtf;
	}
};

// S -> C : notify user list (lobby/room)
struct PktUserListNtf : public PacketHeader
{
	short userCount;
	
	PktUserListNtf()
	{
        memset(this, 0, sizeof(PktUserListNtf));
		packetLength = sizeof(PktUserListNtf);
		type = PacketType::UserListNtf;
	}
};

// C -> S : Request to enter a random room
struct PktEnterRandomRoomReq : public PacketHeader
{
	PktEnterRandomRoomReq()
	{
		memset(this, 0, sizeof(PktEnterRandomRoomReq));
		packetLength = sizeof(PktEnterRandomRoomReq);
		type = PacketType::EnterRandomRoomReq;
	}
};

#pragma pack(pop)
