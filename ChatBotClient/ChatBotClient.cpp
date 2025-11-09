#include "pch.h"
#include "Bot.h"
#include "BotUtility.h"

// =======================================================================
// 메인 함수 (시뮬레이터 시작)
// =======================================================================

int main()
{
    // 1. Winsock 초기화
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
    std::cin.ignore(); // (Enter 키 버퍼 비우기)

    // 2. 봇 객체 및 스레드 생성
    std::vector<std::unique_ptr<Bot>> bots;
    std::vector<std::thread> botThreads;

    for (int i = 0; i < botCount; ++i)
    {
        std::string botID = "Bot_" + std::to_string(i);
        bots.push_back(std::make_unique<Bot>(botID));
    }

    Log("--- " + std::to_string(botCount) + "개의 봇 스레드를 시작합니다... ---");

    // 3. 봇 스레드 시작
    for (auto& bot : bots)
    {
        botThreads.emplace_back(&Bot::Run, bot.get());
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // (접속 부하 분산)
    }

    // 4. 메인 스레드 대기 (종료)
    std::cout << "\n--- 모든 봇이 시작되었습니다. ---" << std::endl;
    std::cout << "--- 서버 부하 테스트 중... ---" << std::endl;
    std::cout << "--- 종료하려면 Enter 키를 누르세요. ---" << std::endl;

    std::string input;
    std::getline(std::cin, input);

    // 5. 종료 처리
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

    // 6. Winsock 정리
    WSACleanup();
    return 0;
}
