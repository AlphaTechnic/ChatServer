#pragma once
#include "pch.h"

// --- Room 클래스 ---
class Room
{
public:
    Room(int id);

    bool AddUser(Session* pSession);
    void RemoveUser(Session* pSession);
    void Broadcast(char* pPacket, int size, SOCKET exceptSocket = INVALID_SOCKET);

    int GetID() const { return m_roomID; }
    int GetUserCount();

private:
    std::mutex m_mutex;
    int m_roomID;
    std::unordered_map<SOCKET, Session*> m_sessions;
};


// --- Lobby 클래스 ---
class Lobby
{
public:
    void AddUser(Session* pSession);
    void RemoveUser(Session* pSession);
    void Broadcast(char* pPacket, int size, SOCKET exceptSocket = INVALID_SOCKET);

private:
    std::mutex m_mutex;
    std::unordered_map<SOCKET, Session*> m_sessions;
};


// --- RoomManager 클래스 ---
class RoomManager
{
public:
    RoomManager() : m_nextRoomID(0) {} // 방 ID 0부터 시작

    Room* CreateRoom();
    Room* GetRoom(int roomID);
    void RemoveRoom(int roomID);
    Room* GetRandomAvailableRoomOrNull();

private:
    std::mutex m_mutex;
    std::unordered_map<int, Room*> m_rooms;
    std::atomic<int> m_nextRoomID;
};


// --- 전역 게임 로직 객체 (외부 선언) ---
extern Lobby g_Lobby;
extern RoomManager g_RoomManager;
