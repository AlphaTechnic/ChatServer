#pragma once
#include "pch.h"

// 인메모리 DB 자료구조
struct ChatLogEntry
{
    std::chrono::system_clock::time_point timestamp;
    int roomID;
    std::string userID;
    std::string message;
};

// --- DB 헬퍼 함수 ---
void LogChatMessage(int roomID, const std::string& userID, const std::string& message);
void CleanupOldChatLogs();
