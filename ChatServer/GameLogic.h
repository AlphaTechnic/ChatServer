#pragma once
#include "pch.h"

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

class RoomManager
{
public:
    RoomManager() : m_nextRoomID(0) {} // start room IDs from 0

    Room* CreateRoom();
    Room* GetRoomOrNull(int roomID);
    void RemoveRoom(int roomID);
    Room* GetRandomAvailableRoomOrNull();

private:
    std::mutex m_mutex;
    std::unordered_map<int, Room*> m_rooms;
    std::atomic<int> m_nextRoomID;
};

extern Lobby g_Lobby;
extern RoomManager g_RoomManager;
