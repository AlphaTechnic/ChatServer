#include "pch.h"
#include "Session.h"

Session::Session(SOCKET s) :
    socket(s),
    currentRoomID(LOBBY_ID),
    isLoggedIn(false),
    currentPacketSize(0),
    lastActivityTime(std::chrono::steady_clock::now())
{
    ZeroMemory(&recvOverlapped, sizeof(OverlappedEx));
    recvOverlapped.operation = IOOperation::Recv;
    recvOverlapped.wsaBuf.buf = recvOverlapped.buffer;
    recvOverlapped.wsaBuf.len = MAX_BUFFER_SIZE;
    ZeroMemory(packetBuffer, sizeof(packetBuffer));
}

void Session::Clear()
{
    isLoggedIn = false;
    currentRoomID = LOBBY_ID;
    userID = "";
    currentPacketSize = 0;
}
