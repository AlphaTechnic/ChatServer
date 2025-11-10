#include "pch.h"
#include "Bot.h"
#include "BotUtility.h"

int main()
{
    // initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed!" << std::endl;
        return 1;
    }

    int botCount = 0;
    while (botCount <= 0 || botCount > 1000)
    {
        std::cout << "생성할 봇의 수(N)를 입력하세요 (1 ~ 1000): ";
        std::cin >> botCount;
    }
    std::cin.ignore();

    // create bot instances and threads
    std::vector<std::unique_ptr<Bot>> bots;
    std::vector<std::thread> botThreads;

    for (int i = 0; i < botCount; ++i)
    {
        std::string botID = "Bot_" + std::to_string(i);
        bots.push_back(std::make_unique<Bot>(botID));
    }

    Log("--- " + std::to_string(botCount) + "개의 봇 스레드를 시작합니다... ---");

    // start bot threads with slight delay to reduce connection load
    for (auto& bot : bots)
    {
        botThreads.emplace_back(&Bot::Run, bot.get());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\n--- 모든 봇이 시작되었습니다. ---" << std::endl;
    std::cout << "--- 서버 부하 테스트 중... ---" << std::endl;
    std::cout << "--- 종료하려면 Enter 키를 누르세요. ---" << std::endl;

    std::string input;
    std::getline(std::cin, input);

    // signal all bots to stop
    Log("--- 봇 종료 신호 전송 중... ---");
    for (auto& bot : bots)
    {
        bot->Stop();
    }

    Log("--- 모든 봇 스레드가 종료되기를 기다리는 중... ---");
    for (auto& th : botThreads)
    {
        if (th.joinable())
        {
            th.join();
        }
    }

    Log("--- 모든 봇이 종료되었습니다. ---");

    // cleanup Winsock
    WSACleanup();
    return 0;
}
