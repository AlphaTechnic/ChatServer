#include "pch.h"
#include "BotNodes.h"
#include "Bot.h"
#include "BotUtility.h"

// leaf nodes

NodeStatus Cond_IsState::Tick(Bot* bot)
{
    return (bot->GetState() == m_targetState) ? NodeStatus::Success : NodeStatus::Failure;
}

NodeStatus Act_TryConnect::Tick(Bot* bot)
{
    Log("[" + bot->GetUserID() + "] (Action) 서버 접속 시도...");
    return bot->TryConnect() ? NodeStatus::Success : NodeStatus::Failure;
}

NodeStatus Act_SendLogin::Tick(Bot* bot)
{
    Log("[" + bot->GetUserID() + "] (Action) 로그인 요청 전송");

    PktLoginReq req;
    strncpy_s(req.userID, bot->GetUserID().c_str(), MAX_USER_ID_LEN);

    bot->SendPacket((char*)&req, req.packetLength);
    return NodeStatus::Success;
}

NodeStatus Act_SendChat::Tick(Bot* bot)
{
    std::string msg = "안녕하세요! (봇 메시지 #" + std::to_string(GetRandomInt(0, 999)) + ")";

    if (m_isLobbyChat)
        Log("[" + bot->GetUserID() + "] (Action) 로비 채팅 전송: " + msg);
    else
        Log("[" + bot->GetUserID() + "] (Action) 방 채팅 전송: " + msg);

    PktChatReq req;
    strncpy_s(req.message, msg.c_str(), MAX_CHAT_LEN);

    bot->SendPacket((char*)&req, req.packetLength);
    return NodeStatus::Success;
}

NodeStatus Act_SendCreateRoom::Tick(Bot* bot)
{
    Log("[" + bot->GetUserID() + "] (Action) 방 생성 요청");

    PktCreateRoomReq req;
    bot->SendPacket((char*)&req, req.packetLength);
    return NodeStatus::Success;
}

NodeStatus Act_SendEnterRandomRoom::Tick(Bot* bot)
{
    Log("[" + bot->GetUserID() + "] (Action) 랜덤 방 입장 요청");

    PktEnterRandomRoomReq req;
    bot->SendPacket((char*)&req, req.packetLength);
    return NodeStatus::Success;
}

NodeStatus Act_SendLeaveRoom::Tick(Bot* bot)
{
    Log("[" + bot->GetUserID() + "] (Action) 방 퇴장 요청");

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
