#pragma once
#include "pch.h"
#include "ServerCore.h"

struct Session
{
    SOCKET socket;
    OverlappedEx recvOverlapped;

    int currentRoomID;
    std::string userID;
    bool isLoggedIn;

    char packetBuffer[MAX_BUFFER_SIZE * 2];
    int currentPacketSize;

    // in order to safely update and read the last activity time across multiple threads
    std::atomic<std::chrono::steady_clock::time_point> lastActivityTime;

    Session(SOCKET s);
    void Clear();
};
