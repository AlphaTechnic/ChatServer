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

#define SERVER_IP "127.0.0.1" // 서버 IP

// --- 전역 변수 및 유틸리티 ---

// 스레드 안전한 로깅을 위한 뮤텍스
std::mutex g_logMutex;

// 스레드 안전한 콘솔 출력
void Log(const std::string& message)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::cout << message << std::endl;
}

// 각 스레드별로 독립적인 난수 생성기
thread_local std::mt19937 g_rng(std::random_device{}() + static_cast<unsigned int>(std::hash<std::thread::id>{}(std::this_thread::get_id())));

// 범위 내 정수 난수 생성
int GetRandomInt(int min, int max)
{
    std::uniform_int_distribution<int> dist(min, max);
    return dist(g_rng);
}

// 범위 내 실수 난수 생성 (확률용)
double GetRandomDouble(double min, double max)
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
    // (참고) 비동기/지연 행동을 위해 'Running'을 추가할 수 있으나,
    // 이 예제에서는 모든 Action을 즉시 실행(Fire-and-forget)으로 간주
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
    // 이 노드를 실행 (Tick)
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
 * 자식 노드 중 하나라도 Success를 반환하면 즉시 Success를 반환.
 * 모든 자식이 Failure를 반환해야 Failure를 반환.
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
 * 모든 자식 노드가 Success를 반환해야 Success를 반환.
 * 하나라도 Failure를 반환하면 즉시 Failure를 반환.
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
 * 요구사항의 "의사 확률 설정"
 * 각 자식 노드에 부여된 가중치(Weight)에 따라 랜덤으로 하나를 선택해 실행.
 * 선택된 자식의 결과를 그대로 반환.
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

        if (totalWeight <= 0.0) // 가중치 합이 0이면 실행 불가
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

        // (비상) 여기까지 오면 안 됨
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

// (Action) 서버에 접속 시도 (유일한 블로킹 Action)
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

// (Action) 방 입장 요청 (테스트를 위해 0, 1, 2번 방 랜덤 입장 시도)
class Act_SendEnterRoom : public Node
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

/**
 * @brief 봇 1개(스레드 1개)를 담당하는 클래스
 */
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
        // (주의) 수신 스레드는 Bot::Run 스레드와 분리되어
        // m_state를 비동기적으로 변경시킴
        std::thread recvTh(&Bot::RecvThread, this);
        recvTh.detach();

        // 3. BT Tick 메인 루프 (봇의 '의지' 담당)
        while (m_isRunning.load())
        {
            if (m_behaviorTree)
            {
                // BT의 루트부터 매번 다시 평가
                m_behaviorTree->Tick(this);
            }

            // 봇의 행동 주기 (너무 빠르지 않게)
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
            return; // 이미 중지됨
        }

        // Recv 스레드가 recv()에서 블록되어 있을 수 있으므로
        // 소켓을 닫아 즉시 종료시킴
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
        // (중요) 상태 변경
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
            // (참고) Send 실패 시 연결이 끊겼을 수 있으므로
            // m_state를 Disconnected로 변경하는 로직이 필요할 수 있음
            Stop(); // 간단하게 봇 중지
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
                // 서버 접속 끊김
                Log("[" + m_userID + "] Server disconnected.");
                m_state.store(BotState::Disconnected); // BT가 다시 접속 시도하도록
                closesocket(m_socket);
                m_socket = INVALID_SOCKET;

                // m_isRunning이 false가 되면 이 스레드도 자동 종료
                if (m_isRunning.load() == false)
                {
                    break;
                }

                // 봇이 살아있다면 재접속을 위해 잠시 대기
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
                continue;
            }

            // (기존 ChatClient와 동일한 패킷 파싱 로직)
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

        // (중요) 서버의 응답에 따라 봇의 상태(m_state)를 변경
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
                // (실패 시 Disconnected로 돌려서 BT가 재시도하게 할 수 있음)
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
                Log("[" + m_userID + "] 방 입장 실패");
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
        {
            PktChatNtf* pNtf = reinterpret_cast<PktChatNtf*>(pPacketData);
            // 봇은 채팅을 받기만 하고 별도 처리는 안 함 (로그만 출력)
            // Log("[" + m_userID + "][RECV] " + pNtf->userID + ": " + pNtf->message);
            break;
        }
        case PacketType::UserEnterNtf:
        case PacketType::UserLeaveNtf:
        case PacketType::UserListNtf:
            // 봇은 유저 리스트 관리를 하지 않으므로 무시
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
        probLobby->AddChild(std::make_unique<Act_SendCreateRoom>(), 10.0); // 10% 방 생성
        probLobby->AddChild(std::make_unique<Act_SendEnterRoom>(), 15.0); // 15% 방 입장
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

    // (중요) 두 개의 스레드(Run, Recv)에서 접근하는 상태 변수
    std::atomic<BotState> m_state;
    std::atomic<int> m_currentRoomID;
    std::atomic<bool> m_isRunning;

    // (Recv 스레드 전용) 패킷 파싱용 버퍼
    char m_packetBuffer[MAX_BUFFER_SIZE * 2];
    int m_currentPacketSize;

    // (Run 스레드 전용) 행동 트리
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
    return NodeStatus::Success; // (주의) 전송 성공이지, 로그인 성공이 아님
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

NodeStatus Act_SendEnterRoom::Tick(Bot* bot)
{
    int targetRoomID = GetRandomInt(0, 2); // 0, 1, 2번 방 중 랜덤 입장 시도
    Log("[" + bot->GetUserID() + "] (Action) 방 입장 요청 (Target: " + std::to_string(targetRoomID) + ")");

    PktEnterRoomReq req;
    req.packetLength = sizeof(req);
    req.type = PacketType::EnterRoomReq;
    req.roomID = targetRoomID;

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
        // 각 봇의 Run() 함수를 새 스레드에서 실행
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
        bot->Stop(); // 각 봇에게 중지 신호
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