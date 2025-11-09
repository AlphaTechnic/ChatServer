#include "pch.h"
#include "Database.h"

// --- 전역 변수 (이 파일 내에서만 사용) ---
static std::deque<ChatLogEntry> g_chatLogDB; // 채팅 로그를 저장할 덱
static std::mutex g_dbMutex;                 // g_chatLogDB 접근 제어를 위한 뮤텍스


/**
 * @brief 채팅 메시지를 인메모리 DB에 저장 (스레드 안전)
 */
void LogChatMessage(int roomID, const std::string& userID, const std::string& message)
{
    std::lock_guard<std::mutex> lock(g_dbMutex); // (중요) DB 접근 잠금

    g_chatLogDB.push_back({
        std::chrono::system_clock::now(), // 현재 시간 (wall clock)
        roomID,
        userID,
        message
        });
}

/**
 * @brief 7일이 지난 오래된 로그를 삭제합니다. (스레드 안전)
 */
void CleanupOldChatLogs()
{
    std::cout << "[DB] Running cleanup for logs older than 7 days..." << std::endl;

    auto sevenDaysAgo = std::chrono::system_clock::now() - std::chrono::hours(24 * 7);
    int logsDeleted = 0;

    std::lock_guard<std::mutex> lock(g_dbMutex); // (중요) DB 접근 잠금

    while (!g_chatLogDB.empty() && g_chatLogDB.front().timestamp < sevenDaysAgo)
    {
        g_chatLogDB.pop_front();
        logsDeleted++;
    }

    std::cout << "[DB] Cleanup complete. " << logsDeleted << " old logs deleted. Total logs: " << g_chatLogDB.size() << std::endl;
}
