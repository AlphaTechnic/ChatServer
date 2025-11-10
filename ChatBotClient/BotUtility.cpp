#include "pch.h"
#include "BotUtility.h"

static std::mutex g_logMutex;

thread_local std::mt19937 g_rng = []() {
    std::random_device rd;
    auto seed = rd() + static_cast<unsigned int>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
    return std::mt19937(seed);
}();


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
