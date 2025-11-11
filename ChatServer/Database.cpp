#include "pch.h"
#include "Database.h"

static std::deque<ChatLogEntry> g_chatLogDB;
static std::mutex g_dbMutex;


// store chat message in the in-memory database
void PersistChatMessage(int roomID, const std::string& userID, const std::string& message)
{
    std::lock_guard<std::mutex> lock(g_dbMutex);

    g_chatLogDB.push_back({
        std::chrono::system_clock::now(),
        roomID,
        userID,
        message
        });
}

// cleanup logs older than 7 days
void CleanupOldChatLogs()
{
    std::cout << "[DB] Running cleanup for logs older than 7 days..." << std::endl;

    auto sevenDaysAgo = std::chrono::system_clock::now() - std::chrono::hours(24 * 7);
    int logsDeleted = 0;

    std::lock_guard<std::mutex> lock(g_dbMutex);

    while (!g_chatLogDB.empty() && g_chatLogDB.front().timestamp < sevenDaysAgo)
    {
        g_chatLogDB.pop_front();
        logsDeleted++;
    }

    std::cout << "[DB] Cleanup complete. " << logsDeleted << " old logs deleted. Total logs: " << g_chatLogDB.size() << std::endl;
}
