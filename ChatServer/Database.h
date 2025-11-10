#pragma once
#include "pch.h"

// In-memory database structure
struct ChatLogEntry
{
    std::chrono::system_clock::time_point timestamp;
    int roomID;
    std::string userID;
    std::string message;
};

void PersistChatMessage(int roomID, const std::string& userID, const std::string& message);
void CleanupOldChatLogs();
