#pragma once
#include "pch.h"
#include "BehaviorTree.h" // Node, NodeStatus

// 봇의 현재 상태
enum class BotState
{
    Dead,            // (초기 상태)
    Disconnected,    // 서버 접속 시도 중
    Connected,       // 접속 완료, 로그인 전
    InLobby,         // 로그인 완료, 로비
    InRoom           // 방에 입장한 상태
};

class Bot
{
public:
    Bot(std::string userID);
    ~Bot();

    // --- 봇 메인 로직 (스레드 진입점) ---
    void Run();

    // --- 봇 종료 ---
    void Stop();

    // --- 네트워크 관련 (BT Action용) ---
    bool TryConnect();
    void SendPacket(char* pPacket, int size);

    // --- 상태 접근자 (Thread-safe) ---
    BotState GetState();
    int GetRoomID();
    const std::string& GetUserID();


private:
    // (Recv 스레드)
    void RecvThread();

    // (Recv 스레드) 수신된 패킷 처리
    void ProcessPacket(char* pPacketData);

    // --- 행동 트리 구성 ---
    void BuildBehaviorTree();

private:
    std::string m_userID;
    SOCKET m_socket;

    std::atomic<BotState> m_state;
    std::atomic<int> m_currentRoomID;
    std::atomic<bool> m_isRunning;

    // (Recv 스레드 전용)
    char m_packetBuffer[MAX_BUFFER_SIZE * 2];
    int m_currentPacketSize;

    // (Run 스레드 전용)
    std::unique_ptr<Node> m_behaviorTree;
};
