#include "pch.h"
#include "BotNodes.h"
#include "Bot.h"
#include "BotUtility.h"

NodeStatus Cond_IsState::Tick(Bot* bot)
{
    return (bot->GetState() == m_targetState) ? NodeStatus::Success : NodeStatus::Failure;
}

NodeStatus Act_TryConnect::Tick(Bot* bot)
{
    Log("[" + bot->GetUserID() + "] (Action) Attempting to connect to server...");
    return bot->TryConnect() ? NodeStatus::Success : NodeStatus::Failure;
}

NodeStatus Act_SendLogin::Tick(Bot* bot)
{
    Log("[" + bot->GetUserID() + "] (Action) Sending login request");

    PktLoginReq req;
    strncpy_s(req.userID, bot->GetUserID().c_str(), MAX_USER_ID_LEN);

    bot->SendPacket((char*)&req, req.packetLength);
    return NodeStatus::Success;
}

NodeStatus Act_SendChat::Tick(Bot* bot)
{
    std::string msg = "Hello! (Bot message #" + std::to_string(GetRandomInt(0, 999)) + ")";

    if (m_isLobbyChat)
        Log("[" + bot->GetUserID() + "] (Action) Lobby chat sent: " + msg);
    else
        Log("[" + bot->GetUserID() + "] (Action) Room chat sent: " + msg);

    PktChatReq req;
    strncpy_s(req.message, msg.c_str(), MAX_CHAT_LEN);

    bot->SendPacket((char*)&req, req.packetLength);
    return NodeStatus::Success;
}

NodeStatus Act_SendCreateRoom::Tick(Bot* bot)
{
    Log("[" + bot->GetUserID() + "] (Action) Sending create room request");

    PktCreateRoomReq req;
    bot->SendPacket((char*)&req, req.packetLength);
    return NodeStatus::Success;
}

NodeStatus Act_SendEnterRandomRoom::Tick(Bot* bot)
{
    Log("[" + bot->GetUserID() + "] (Action) Sending enter random room request");

    PktEnterRandomRoomReq req;
    bot->SendPacket((char*)&req, req.packetLength);
    return NodeStatus::Success;
}

NodeStatus Act_SendLeaveRoom::Tick(Bot* bot)
{
    Log("[" + bot->GetUserID() + "] (Action) Sending leave room request");

    PktLeaveRoomReq req;
    bot->SendPacket((char*)&req, req.packetLength);
    return NodeStatus::Success;
}

NodeStatus Act_Wait::Tick(Bot* bot)
{
    int waitTime = GetRandomInt(m_minMs, m_maxMs);
    std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
    return NodeStatus::Success;
}
