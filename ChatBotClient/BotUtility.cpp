#include "pch.h"
#include "BotUtility.h"

// 스레드 안전한 로깅을 위한 뮤텍스
static std::mutex g_logMutex;

// 각 스레드별로 독립적인 난수 생성기
thread_local std::mt19937 g_rng(std::random_device{}() + static_cast<unsigned int>(std::hash<std::thread::id>{}(std::this_thread::get_id())));


void Log(const std::string& message)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::cout << message << std::endl;
}

int GetRandomInt(int min, int max)
{
    std::uniform_int_distribution<int> dist(min, max);
    return dist(g_rng);
}

double GetRandomDouble(double min, double max)
{
    std::uniform_real_distribution<double> dist(min, max);
    return dist(g_rng);
}
