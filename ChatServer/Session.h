#pragma once
#include "pch.h"
#include "ServerCore.h" // OverlappedEx

// 클라이언트 세션 구조체
struct Session
{
    SOCKET socket;
    OverlappedEx recvOverlapped; // Recv 전용 OverlappedEx

    int currentRoomID;
    std::string userID;
    bool isLoggedIn;

    // 패킷 조립을 위한 버퍼
    char packetBuffer[MAX_BUFFER_SIZE * 2];
    int currentPacketSize; // 현재까지 조립된 패킷 크기

    // 마지막 활동 시간을 기록 (원자적 접근)
    std::atomic<std::chrono::steady_clock::time_point> lastActivityTime;

    Session(SOCKET s); // 생성자
    void Clear();      // 세션 정리
};
