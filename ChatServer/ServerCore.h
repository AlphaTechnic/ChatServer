#pragma once
#include "pch.h"

constexpr int SESSION_TIMEOUT_SECONDS = 60;
constexpr int TIMEOUT_CHECK_INTERVAL_MS = 5000;

constexpr int DB_CLEANUP_INTERVAL_HOURS = 1;

enum class IOOperation
{
    Recv,
    Send
};

struct OverlappedEx
{
    OVERLAPPED overlapped;
    IOOperation operation;
    WSABUF wsaBuf;
    char buffer[MAX_BUFFER_SIZE];
};
