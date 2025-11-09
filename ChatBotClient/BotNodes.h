#pragma once
#include "BehaviorTree.h" // Node 상속
#include "Bot.h"

// --- Leaf Nodes (실제 행동/조건 노드) ---

// (Condition) 봇이 특정 상태인지 확인
class Cond_IsState : public Node
{
public:
    Cond_IsState(BotState targetState) : m_targetState(targetState) {}
    virtual NodeStatus Tick(Bot* bot) override;
private:
    BotState m_targetState;
};

// (Action) 서버에 접속 시도
class Act_TryConnect : public Node
{
public:
    virtual NodeStatus Tick(Bot* bot) override;
};

// (Action) 로그인 패킷 전송
class Act_SendLogin : public Node
{
public:
    virtual NodeStatus Tick(Bot* bot) override;
};

// (Action) (로비/방) 채팅 전송
class Act_SendChat : public Node
{
public:
    Act_SendChat(bool isLobbyChat) : m_isLobbyChat(isLobbyChat) {}
    virtual NodeStatus Tick(Bot* bot) override;
private:
    bool m_isLobbyChat;
};

// (Action) 방 생성 요청
class Act_SendCreateRoom : public Node
{
public:
    virtual NodeStatus Tick(Bot* bot) override;
};

// (Action) 랜덤 방 입장 요청
class Act_SendEnterRandomRoom : public Node
{
public:
    virtual NodeStatus Tick(Bot* bot) override;
};

// (Action) 방 퇴장 요청
class Act_SendLeaveRoom : public Node
{
public:
    virtual NodeStatus Tick(Bot* bot) override;
};

// (Action) 랜덤 시간 동안 대기
class Act_Wait : public Node
{
public:
    Act_Wait(int minMs, int maxMs) : m_minMs(minMs), m_maxMs(maxMs) {}

    virtual NodeStatus Tick(Bot* bot) override;
private:
    int m_minMs, m_maxMs;
};

// (Action) 아무것도 안 함 (Idle)
class Act_DoNothing : public Node
{
public:
    virtual NodeStatus Tick(Bot* bot) override
    {
        return NodeStatus::Success; // 구현이 간단해 헤더에 바로 정의
    }
};
