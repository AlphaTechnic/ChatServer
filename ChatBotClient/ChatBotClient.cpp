#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <vector>
#include <string>
#include <memory>       // for std::unique_ptr
#include <atomic>       // for std::atomic
#include <mutex>        // for std::mutex (로깅용)
#include <random>       // for std::mt19937
#include <chrono>       // for std::this_thread::sleep_for
#include <numeric>      // for std::iota

#include "Protocol.h" // 공통 프로토콜 헤더

#pragma comment(lib, "ws2_32.lib")

// --- 전역 변수 및 유틸리티 ---

// 스레드 안전한 로깅을 위한 뮤텍스
std::mutex g_logMutex;

// 스레드 안전한 콘솔 출력
static void Log(const std::string& message)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::cout << message << std::endl;
}

// 각 스레드별로 독립적인 난수 생성기
thread_local std::mt19937 g_rng(std::random_device{}() + static_cast<unsigned int>(std::hash<std::thread::id>{}(std::this_thread::get_id())));

// 범위 내 정수 난수 생성
static int GetRandomInt(int min, int max)
{
    std::uniform_int_distribution<int> dist(min, max);
    return dist(g_rng);
}

// 범위 내 실수 난수 생성 (확률용)
static double GetRandomDouble(double min, double max)
{
    std::uniform_real_distribution<double> dist(min, max);
    return dist(g_rng);
}


// =======================================================================
// 1. 봇(Bot) 클래스 및 상태 정의
// =======================================================================

// 봇의 행동 트리 노드가 반환할 상태
enum class NodeStatus
{
    Success,
    Failure
};

// 봇의 현재 상태
enum class BotState
{
    Dead,           // (초기 상태)
    Disconnected,   // 서버 접속 시도 중
    Connected,      // 접속 완료, 로그인 전
    InLobby,        // 로그인 완료, 로비
    InRoom          // 방에 입장한 상태
};

// 봇의 모든 행동 트리를 구성할 기본 노드 (Interface)
class Bot; // 전방 선언
class Node
{
public:
    virtual ~Node() = default;
    virtual NodeStatus Tick(Bot* bot) = 0;
};

// =======================================================================
// 2. 행동 트리(BT) 프레임워크 구현
// =======================================================================

// --- 2-1. Composite Nodes (자식 노드를 가지는 노드) ---

class CompositeNode : public Node
{
public:
    void AddChild(std::unique_ptr<Node> child)
    {
        m_children.push_back(std::move(child));
    }

protected:
    std::vector<std::unique_ptr<Node>> m_children;
};

/**
 * @brief (Selector 노드 - 'OR' 연산)
 */
class Selector : public CompositeNode
{
public:
    virtual NodeStatus Tick(Bot* bot) override
    {
        for (auto& child : m_children)
        {
            if (child->Tick(bot) == NodeStatus::Success)
            {
                return NodeStatus::Success;
            }
        }
        return NodeStatus::Failure;
    }
};

/**
 * @brief (Sequence 노드 - 'AND' 연산)
 */
class Sequence : public CompositeNode
{
public:
    virtual NodeStatus Tick(Bot* bot) override
    {
        for (auto& child : m_children)
        {
            if (child->Tick(bot) == NodeStatus::Failure)
            {
                return NodeStatus::Failure;
            }
        }
        return NodeStatus::Success;
    }
};


/**
 * @brief (Probabilistic Selector 노드 - '확률적 OR' 연산)
 */
class ProbabilisticSelector : public CompositeNode
{
public:
    // 가중치와 함께 자식 추가
    void AddChild(std::unique_ptr<Node> child, double weight)
    {
        CompositeNode::AddChild(std::move(child));
        m_weights.push_back(weight);
    }

    virtual NodeStatus Tick(Bot* bot) override
    {
        if (m_children.empty())
        {
            return NodeStatus::Success;
        }

        double totalWeight = 0.0;
        for (double w : m_weights)
        {
            totalWeight += w;
        }

        if (totalWeight <= 0.0)
        {
            return NodeStatus::Failure;
        }

        double roll = GetRandomDouble(0.0, totalWeight);

        double currentSum = 0.0;
        for (size_t i = 0; i < m_children.size(); ++i)
        {
            currentSum += m_weights[i];
            if (roll < currentSum)
            {
                return m_children[i]->Tick(bot); // 선택된 자식 실행
            }
        }
        return m_children.back()->Tick(bot);
    }

private:
    std::vector<double> m_weights;
};


// --- 2-2. Leaf Nodes (실제 행동/조건 노드) ---

// (Condition) 봇이 특정 상태인지 확인
class Cond_IsState : public Node
{
public:
    Cond_IsState(BotState targetState) : m_targetState(targetState) {}

    virtual NodeStatus Tick(Bot* bot) override; // Bot 클래스 정의 후 구현

private:
    BotState m_targetState;
};

// (Action) 서버에 접속 시도
class Act_TryConnect : public Node
{
public:
    virtual NodeStatus Tick(Bot* bot) override;
};

// (Action) 로그인 패킷 전송
class Act_SendLogin : public Node
{
public:
    virtual NodeStatus Tick(Bot* bot) override;
};

// (Action) (로비/방) 채팅 전송
class Act_SendChat : public Node
{
public:
    Act_SendChat(bool isLobbyChat) : m_isLobbyChat(isLobbyChat) {}
    virtual NodeStatus Tick(Bot* bot) override;
private:
    bool m_isLobbyChat;
};

// (Action) 방 생성 요청
class Act_SendCreateRoom : public Node
{
public:
    virtual NodeStatus Tick(Bot* bot) override;
};

// [업데이트]
// (Action) 랜덤 방 입장 요청 (서버에 매칭 요청)
class Act_SendEnterRandomRoom : public Node
{
public:
    virtual NodeStatus Tick(Bot* bot) override;
};

// (Action) 방 퇴장 요청
class Act_SendLeaveRoom : public Node
{
public:
    virtual NodeStatus Tick(Bot* bot) override;
};

// (Action) 랜덤 시간 동안 대기
class Act_Wait : public Node
{
public:
    Act_Wait(int minMs, int maxMs) : m_minMs(minMs), m_maxMs(maxMs) {}

    virtual NodeStatus Tick(Bot* bot) override
    {
        int waitTime = GetRandomInt(m_minMs, m_maxMs);
        std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
        return NodeStatus::Success;
    }
private:
    int m_minMs, m_maxMs;
};

// (Action) 아무것도 안 함 (Idle)
class Act_DoNothing : public Node
{
public:
    virtual NodeStatus Tick(Bot* bot) override
    {
        return NodeStatus::Success;
    }
};


// =======================================================================
// 3. 봇(Bot) 클래스 구현
// =======================================================================

class Bot
{
public:
    Bot(std::string userID)
        : m_userID(userID),
        m_socket(INVALID_SOCKET),
        m_state(BotState::Dead),
        m_currentRoomID(LOBBY_ID),
        m_currentPacketSize(0),
        m_isRunning(false)
    {
        ZeroMemory(m_packetBuffer, sizeof(m_packetBuffer));
    }

    ~Bot()
    {
        Stop();
    }

    // --- 봇 메인 로직 (스레드 진입점) ---
    void Run()
    {
        m_isRunning = true;
        m_state.store(BotState::Disconnected);

        // 1. 행동 트리 구성
        BuildBehaviorTree();

        // 2. 수신 스레드 시작
        std::thread recvTh(&Bot::RecvThread, this);
        recvTh.detach();

        // 3. BT Tick 메인 루프 (봇의 '의지' 담당)
        while (m_isRunning.load())
        {
            if (m_behaviorTree)
            {
                m_behaviorTree->Tick(this);
            }

            // 봇의 행동 주기
            int thinkTime = GetRandomInt(500, 2000); // 0.5초 ~ 2초
            std::this_thread::sleep_for(std::chrono::milliseconds(thinkTime));
        }

        Log("[" + m_userID + "] Bot logic loop stopped.");
    }

    // --- 봇 종료 ---
    void Stop()
    {
        if (!m_isRunning.exchange(false))
        {
            return;
        }
        if (m_socket != INVALID_SOCKET)
        {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
    }


    // --- 네트워크 관련 ---

    // (BT Action용) 서버 접속
    bool TryConnect()
    {
        if (m_socket != INVALID_SOCKET)
        {
            closesocket(m_socket);
        }

        m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == INVALID_SOCKET)
        {
            Log("[" + m_userID + "] Socket creation failed!");
            return false;
        }

        sockaddr_in serverAddr;
        ZeroMemory(&serverAddr, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

        if (connect(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
        {
            Log("[" + m_userID + "] Connect failed!");
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            return false;
        }

        Log("[" + m_userID + "] Connected to server.");
        m_state.store(BotState::Connected);
        return true;
    }

    // (BT Action용) 패킷 전송
    void SendPacket(char* pPacket, int size)
    {
        if (m_socket == INVALID_SOCKET || !m_isRunning.load())
        {
            return;
        }

        if (send(m_socket, pPacket, size, 0) == SOCKET_ERROR)
        {
            Log("[" + m_userID + "] Send failed!");
            Stop();
        }
    }

    // --- 상태 접근자 (Thread-safe) ---
    BotState GetState() { return m_state.load(); }
    int GetRoomID() { return m_currentRoomID.load(); }
    const std::string& GetUserID() { return m_userID; }


private:
    // (Recv 스레드)
    void RecvThread()
    {
        char recvBuffer[MAX_BUFFER_SIZE];

        while (m_isRunning.load())
        {
            if (m_socket == INVALID_SOCKET)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            int nRecv = recv(m_socket, recvBuffer, MAX_BUFFER_SIZE, 0);
            if (nRecv <= 0)
            {
                Log("[" + m_userID + "] Server disconnected.");
                m_state.store(BotState::Disconnected);
                closesocket(m_socket);
                m_socket = INVALID_SOCKET;

                if (m_isRunning.load() == false)
                {
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
                continue;
            }

            // 패킷 파싱 로직
            memcpy(m_packetBuffer + m_currentPacketSize, recvBuffer, nRecv);
            m_currentPacketSize += nRecv;

            while (m_currentPacketSize >= sizeof(PacketHeader))
            {
                PacketHeader* pHeader = reinterpret_cast<PacketHeader*>(m_packetBuffer);
                if (m_currentPacketSize >= pHeader->packetLength)
                {
                    ProcessPacket(m_packetBuffer);

                    int remainingSize = m_currentPacketSize - pHeader->packetLength;
                    if (remainingSize > 0)
                    {
                        memmove(m_packetBuffer, m_packetBuffer + pHeader->packetLength, remainingSize);
                    }
                    m_currentPacketSize = remainingSize;
                }
                else
                {
                    break;
                }
            }
        }
        Log("[" + m_userID + "] Recv thread stopped.");
    }

    // (Recv 스레드) 수신된 패킷 처리
    void ProcessPacket(char* pPacketData)
    {
        PacketHeader* pHeader = reinterpret_cast<PacketHeader*>(pPacketData);

        switch (pHeader->type)
        {
        case PacketType::LoginRes:
        {
            PktLoginRes* pRes = reinterpret_cast<PktLoginRes*>(pPacketData);
            if (pRes->success)
            {
                Log("[" + m_userID + "] 로그인 성공 -> InLobby");
                m_state.store(BotState::InLobby);
            }
            else
            {
                Log("[" + m_userID + "] 로그인 실패");
            }
            break;
        }
        case PacketType::CreateRoomRes:
        {
            PktCreateRoomRes* pRes = reinterpret_cast<PktCreateRoomRes*>(pPacketData);
            if (pRes->success)
            {
                Log("[" + m_userID + "] 방 생성 성공 (Room " + std::to_string(pRes->newRoomID) + ") -> InRoom");
                m_currentRoomID.store(pRes->newRoomID);
                m_state.store(BotState::InRoom);
            }
            else
            {
                Log("[" + m_userID + "] 방 생성 실패");
            }
            break;
        }
        // [업데이트]
        // 랜덤 입장이든, 지정 입장이든 서버는 PktEnterRoomRes로 응답함
        // 따라서 이 로직은 수정할 필요가 없음 (서버의 응답을 그대로 처리)
        case PacketType::EnterRoomRes:
        {
            PktEnterRoomRes* pRes = reinterpret_cast<PktEnterRoomRes*>(pPacketData);
            if (pRes->success)
            {
                Log("[" + m_userID + "] 방 입장 성공 (Room " + std::to_string(pRes->roomID) + ") -> InRoom");
                m_currentRoomID.store(pRes->roomID);
                m_state.store(BotState::InRoom);
            }
            else
            {
                // (PktEnterRandomRoomReq에 대한 실패 응답도 여기로 옴)
                Log("[" + m_userID + "] 방 입장 실패 (방이 없거나/꽉 찼거나/입장 가능한 방 없음)");
            }
            break;
        }
        case PacketType::LeaveRoomRes:
        {
            PktLeaveRoomRes* pRes = reinterpret_cast<PktLeaveRoomRes*>(pPacketData);
            if (pRes->success)
            {
                Log("[" + m_userID + "] 방 퇴장 성공 -> InLobby");
                m_currentRoomID.store(LOBBY_ID);
                m_state.store(BotState::InLobby);
            }
            break;
        }
        case PacketType::ChatNtf:
        case PacketType::UserEnterNtf:
        case PacketType::UserLeaveNtf:
        case PacketType::UserListNtf:
            // 봇은 다른 유저 정보나 채팅 수신을 무시
            break;

        default:
            break;
        }
    }


    // --- 행동 트리 구성 ---
    void BuildBehaviorTree()
    {
        // 최상위 루트
        auto root = std::make_unique<Selector>();

        // 1. (Disconnected 상태) -> 접속 시도
        auto seqConnect = std::make_unique<Sequence>();
        seqConnect->AddChild(std::make_unique<Cond_IsState>(BotState::Disconnected));
        seqConnect->AddChild(std::make_unique<Act_TryConnect>());
        root->AddChild(std::move(seqConnect));

        // 2. (Connected 상태) -> 로그인 시도
        auto seqLogin = std::make_unique<Sequence>();
        seqLogin->AddChild(std::make_unique<Cond_IsState>(BotState::Connected));
        seqLogin->AddChild(std::make_unique<Act_SendLogin>());
        root->AddChild(std::move(seqLogin));

        // 3. (InLobby 상태) -> 로비 행동 결정
        auto seqLobby = std::make_unique<Sequence>();
        seqLobby->AddChild(std::make_unique<Cond_IsState>(BotState::InLobby));
        seqLobby->AddChild(std::make_unique<Act_Wait>(1000, 3000)); // 행동 전 1~3초 대기

        auto probLobby = std::make_unique<ProbabilisticSelector>(); // 확률 노드
        probLobby->AddChild(std::make_unique<Act_SendChat>(true), 70.0); // 70% 로비 채팅
        probLobby->AddChild(std::make_unique<Act_SendCreateRoom>(), 15.0); // 15% 방 생성
        // [업데이트] Act_SendEnterRoom -> Act_SendEnterRandomRoom
        probLobby->AddChild(std::make_unique<Act_SendEnterRandomRoom>(), 10.0); // 10% 랜덤 방 입장
        probLobby->AddChild(std::make_unique<Act_DoNothing>(), 5.0); // 5% 아무것도 안함

        seqLobby->AddChild(std::move(probLobby));
        root->AddChild(std::move(seqLobby));

        // 4. (InRoom 상태) -> 방 행동 결정
        auto seqRoom = std::make_unique<Sequence>();
        seqRoom->AddChild(std::make_unique<Cond_IsState>(BotState::InRoom));
        seqRoom->AddChild(std::make_unique<Act_Wait>(1000, 5000)); // 행동 전 1~5초 대기

        auto probRoom = std::make_unique<ProbabilisticSelector>(); // 확률 노드
        probRoom->AddChild(std::make_unique<Act_SendChat>(false), 80.0); // 80% 방 채팅
        probRoom->AddChild(std::make_unique<Act_SendLeaveRoom>(), 15.0); // 15% 방 나가기
        probRoom->AddChild(std::make_unique<Act_DoNothing>(), 5.0); // 5% 아무것도 안함

        seqRoom->AddChild(std::move(probRoom));
        root->AddChild(std::move(seqRoom));

        // 5. 트리를 봇에 장착
        m_behaviorTree = std::move(root);
    }


private:
    std::string m_userID;
    SOCKET m_socket;

    std::atomic<BotState> m_state;
    std::atomic<int> m_currentRoomID;
    std::atomic<bool> m_isRunning;

    // (Recv 스레드 전용)
    char m_packetBuffer[MAX_BUFFER_SIZE * 2];
    int m_currentPacketSize;

    // (Run 스레드 전용)
    std::unique_ptr<Node> m_behaviorTree;
};


// =======================================================================
// 4. BT Leaf Nodes 구현 (Bot 클래스 정의 이후)
// =======================================================================

NodeStatus Cond_IsState::Tick(Bot* bot)
{
    return (bot->GetState() == m_targetState) ? NodeStatus::Success : NodeStatus::Failure;
}

NodeStatus Act_TryConnect::Tick(Bot* bot)
{
    Log("[" + bot->GetUserID() + "] (Action) 서버 접속 시도...");
    return bot->TryConnect() ? NodeStatus::Success : NodeStatus::Failure;
}

NodeStatus Act_SendLogin::Tick(Bot* bot)
{
    Log("[" + bot->GetUserID() + "] (Action) 로그인 요청 전송");

    PktLoginReq req;
    req.packetLength = sizeof(req);
    req.type = PacketType::LoginReq;
    strncpy_s(req.userID, bot->GetUserID().c_str(), MAX_USER_ID_LEN);

    bot->SendPacket((char*)&req, req.packetLength);
    return NodeStatus::Success;
}

NodeStatus Act_SendChat::Tick(Bot* bot)
{
    std::string msg = "안녕하세요! (봇 메시지 #" + std::to_string(GetRandomInt(0, 999)) + ")";

    if (m_isLobbyChat)
        Log("[" + bot->GetUserID() + "] (Action) 로비 채팅 전송: " + msg);
    else
        Log("[" + bot->GetUserID() + "] (Action) 방 채팅 전송: " + msg);

    PktChatReq req;
    req.packetLength = sizeof(req);
    req.type = PacketType::ChatReq;
    strncpy_s(req.message, msg.c_str(), MAX_CHAT_LEN);

    bot->SendPacket((char*)&req, req.packetLength);
    return NodeStatus::Success;
}

NodeStatus Act_SendCreateRoom::Tick(Bot* bot)
{
    Log("[" + bot->GetUserID() + "] (Action) 방 생성 요청");

    PktCreateRoomReq req;
    req.packetLength = sizeof(req);
    req.type = PacketType::CreateRoomReq;

    bot->SendPacket((char*)&req, req.packetLength);
    return NodeStatus::Success;
}

// [업데이트]
NodeStatus Act_SendEnterRandomRoom::Tick(Bot* bot)
{
    Log("[" + bot->GetUserID() + "] (Action) 랜덤 방 입장 요청");

    PktEnterRandomRoomReq req;
    req.packetLength = sizeof(req);
    req.type = PacketType::EnterRandomRoomReq;

    bot->SendPacket((char*)&req, req.packetLength);
    return NodeStatus::Success;
}

NodeStatus Act_SendLeaveRoom::Tick(Bot* bot)
{
    Log("[" + bot->GetUserID() + "] (Action) 방 퇴장 요청");

    PktLeaveRoomReq req;
    req.packetLength = sizeof(req);
    req.type = PacketType::LeaveRoomReq;

    bot->SendPacket((char*)&req, req.packetLength);
    return NodeStatus::Success;
}


// =======================================================================
// 5. 메인 함수 (시뮬레이터 시작)
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