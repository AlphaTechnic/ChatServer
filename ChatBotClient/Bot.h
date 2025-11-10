#pragma once
#include "pch.h"
#include "BehaviorTree.h"

enum class BotState
{
    Dead,
    Disconnected,
    Connected,
    InLobby,
    InRoom
};

class Bot
{
public:
    Bot(std::string userID);
    ~Bot();

    void Run();
    void Stop();

    // network related functions (for BT Actions)
    bool TryConnect();
    void SendPacket(char* pPacket, int size);

    // accessors
    BotState GetState();
    int GetRoomID();
    const std::string& GetUserID();


private:
    void RecvThread();
    void ProcessPacket(char* pPacketData);
    void BuildBehaviorTree();

private:
    std::string m_userID;
    SOCKET m_socket;

    std::atomic<BotState> m_state;
    std::atomic<int> m_currentRoomID;
    std::atomic<bool> m_isRunning;

    // used for RecevThread
    char m_packetBuffer[MAX_BUFFER_SIZE * 2];
    int m_currentPacketSize;

    // used for RunThread
    std::unique_ptr<Node> m_behaviorTree;
};
