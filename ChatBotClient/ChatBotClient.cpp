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
        std::cout << "Enter the number of bots to create (1 ~ 1000): ";
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

    Log("--- Starting " + std::to_string(botCount) + " bot threads... ---");

    // start bot threads with slight delay to reduce connection load
    for (auto& bot : bots)
    {
        botThreads.emplace_back(&Bot::Run, bot.get());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\n--- All bots have started. ---" << std::endl;
    std::cout << "--- Stress testing the server... ---" << std::endl;
    std::cout << "--- Press Enter key to stop. ---" << std::endl;

    std::string input;
    std::getline(std::cin, input);

    // signal all bots to stop
    Log("--- Sending stop signal to all bots... ---");
    for (auto& bot : bots)
    {
        bot->Stop();
    }

    Log("--- Waiting for all bot threads to terminate... ---");
    for (auto& th : botThreads)
    {
        if (th.joinable())
        {
            th.join();
        }
    }

    Log("--- All bots have terminated. ---");

    // cleanup Winsock
    WSACleanup();
    return 0;
}
