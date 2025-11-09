#pragma once
#include "pch.h" // MAX_BUFFER_SIZE 등을 위해

// --- 타임아웃 설정 ---
constexpr int SESSION_TIMEOUT_SECONDS = 60;
constexpr int TIMEOUT_CHECK_INTERVAL_MS = 5000;

// --- DB 정리 주기 ---
constexpr int DB_CLEANUP_INTERVAL_HOURS = 1;

// I/O 작업의 종류를 구분하기 위한 열거형
enum class IOOperation
{
    Recv,
    Send
};

// OVERLAPPED 구조체를 확장
struct OverlappedEx
{
    OVERLAPPED overlapped;
    IOOperation operation;
    WSABUF wsaBuf;
    char buffer[MAX_BUFFER_SIZE]; // Protocol.h에 정의된 MAX_BUFFER_SIZE
};
