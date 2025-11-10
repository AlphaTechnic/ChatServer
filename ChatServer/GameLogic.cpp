#include "pch.h"
#include "GameLogic.h"
#include "PacketHandler.h"
#include "Session.h"

Lobby g_Lobby;
RoomManager g_RoomManager;

// Room
Room::Room(int id) : m_roomID(id) {}

bool Room::AddUser(Session* pSession)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_sessions.size() >= MAX_ROOM_USERS)
    {
        return false;
    }

    m_sessions[pSession->socket] = pSession;
    pSession->currentRoomID = m_roomID;

    std::cout << "[Room " << m_roomID << "] User '" << pSession->userID << "' entered. (Total: " << m_sessions.size() << ")" << std::endl;

    return true;
}

void Room::RemoveUser(Session* pSession)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessions.erase(pSession->socket);
    pSession->currentRoomID = LOBBY_ID;

    std::cout << "[Room " << m_roomID << "] User '" << pSession->userID << "' left. (Total: " << m_sessions.size() << ")" << std::endl;
}

void Room::Broadcast(char* pPacket, int size, SOCKET exceptSocket)
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

int Room::GetUserCount()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_sessions.size());
}


// Lobby
void Lobby::AddUser(Session* pSession)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessions[pSession->socket] = pSession;
    pSession->currentRoomID = LOBBY_ID;

    std::cout << "[Lobby] User '" << pSession->userID << "' entered. (Total: " << m_sessions.size() << ")" << std::endl;
}

void Lobby::RemoveUser(Session* pSession)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessions.erase(pSession->socket);

    std::cout << "[Lobby] User '" << pSession->userID << "' left. (Total: " << m_sessions.size() << ")" << std::endl;
}

void Lobby::Broadcast(char* pPacket, int size, SOCKET exceptSocket)
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


// RoomManager
Room* RoomManager::CreateRoom()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    int newRoomID = m_nextRoomID++;
    Room* pRoom = new Room(newRoomID);
    m_rooms[newRoomID] = pRoom;

    std::cout << "[System] Room " << newRoomID << " created." << std::endl;
    return pRoom;
}

Room* RoomManager::GetRoomOrNull(int roomID)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_rooms.find(roomID);
    if (it != m_rooms.end())
    {
        return it->second;
    }
    return nullptr;
}

void RoomManager::RemoveRoom(int roomID)
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

Room* RoomManager::GetRandomAvailableRoomOrNull()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<Room*> availableRooms;
    availableRooms.reserve(m_rooms.size());

    for (auto& pair : m_rooms)
    {
        if (pair.second->GetUserCount() < MAX_ROOM_USERS)
        {
            availableRooms.push_back(pair.second);
        }
    }

    if (availableRooms.empty())
    {
        return nullptr;
    }

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, static_cast<int>(availableRooms.size() - 1));

    return availableRooms[dis(gen)];
}
