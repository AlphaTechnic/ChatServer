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

    // buffer to assemble packets
    char packetBuffer[MAX_BUFFER_SIZE * 2];
    // current size of data in the buffer
    int currentPacketSize;

    // in order to safely update and read the last activity time across multiple threads, we use std::atomic
    std::atomic<std::chrono::steady_clock::time_point> lastActivityTime;

    Session(SOCKET s);
    void Clear();
};
