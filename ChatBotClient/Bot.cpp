#include "pch.h"
#include "Bot.h"
#include "BotUtility.h" // Log()
#include "BotNodes.h"   // BuildBehaviorTree()에서 사용

Bot::Bot(std::string userID)
    : m_userID(userID),
    m_socket(INVALID_SOCKET),
    m_state(BotState::Dead),
    m_currentRoomID(LOBBY_ID),
    m_currentPacketSize(0),
    m_isRunning(false)
{
    ZeroMemory(m_packetBuffer, sizeof(m_packetBuffer));
}

Bot::~Bot()
{
    Stop();
}

// --- 봇 메인 로직 (스레드 진입점) ---
void Bot::Run()
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

        int thinkTime = GetRandomInt(500, 2000); // 0.5초 ~ 2초
        std::this_thread::sleep_for(std::chrono::milliseconds(thinkTime));
    }

    Log("[" + m_userID + "] Bot logic loop stopped.");
}

// --- 봇 종료 ---
void Bot::Stop()
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

bool Bot::TryConnect()
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

void Bot::SendPacket(char* pPacket, int size)
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
BotState Bot::GetState() { return m_state.load(); }
int Bot::GetRoomID() { return m_currentRoomID; }
const std::string& Bot::GetUserID() { return m_userID; }


// --- Private Methods ---

void Bot::RecvThread()
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
            std::this_thread::sleep_for(std::chrono::milliseconds(3000)); // 재접속 대기
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

void Bot::ProcessPacket(char* pPacketData)
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

void Bot::BuildBehaviorTree()
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
