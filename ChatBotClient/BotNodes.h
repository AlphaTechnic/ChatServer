#pragma once
#include "BehaviorTree.h"
#include "Bot.h"

// leaf nodes

class Cond_IsState : public Node
{
public:
    Cond_IsState(BotState targetState) : m_targetState(targetState) {}
    virtual NodeStatus Tick(Bot* bot) override;
private:
    BotState m_targetState;
};

class Act_TryConnect : public Node
{
public:
    virtual NodeStatus Tick(Bot* bot) override;
};

class Act_SendLogin : public Node
{
public:
    virtual NodeStatus Tick(Bot* bot) override;
};

class Act_SendChat : public Node
{
public:
    Act_SendChat(bool isLobbyChat) : m_isLobbyChat(isLobbyChat) {}
    virtual NodeStatus Tick(Bot* bot) override;
private:
    bool m_isLobbyChat;
};

class Act_SendCreateRoom : public Node
{
public:
    virtual NodeStatus Tick(Bot* bot) override;
};

class Act_SendEnterRandomRoom : public Node
{
public:
    virtual NodeStatus Tick(Bot* bot) override;
};

class Act_SendLeaveRoom : public Node
{
public:
    virtual NodeStatus Tick(Bot* bot) override;
};

class Act_Wait : public Node
{
public:
    Act_Wait(int minMs, int maxMs) : m_minMs(minMs), m_maxMs(maxMs) {}

    virtual NodeStatus Tick(Bot* bot) override;
private:
    int m_minMs, m_maxMs;
};

class Act_DoNothing : public Node
{
public:
    virtual NodeStatus Tick(Bot* bot) override
    {
        return NodeStatus::Success;
    }
};
