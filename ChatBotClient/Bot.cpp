#include "pch.h"
#include "Bot.h"
#include "BotUtility.h"
#include "BotNodes.h"

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

void Bot::Run()
{
    m_isRunning = true;
    m_state.store(BotState::Disconnected);

    BuildBehaviorTree();

    // start the receive thread
    std::thread recvTh(&Bot::RecvThread, this);
    recvTh.detach();

    // bot's 'will' handler
    while (m_isRunning.load())
    {
        if (m_behaviorTree)
        {
            m_behaviorTree->Tick(this);
        }

        int thinkTime = GetRandomInt(500, 2000); // 0.5 sec ~ 2 sec
        std::this_thread::sleep_for(std::chrono::milliseconds(thinkTime));
    }

    Log("[" + m_userID + "] Bot logic loop stopped.");
}

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

// state accessors
BotState Bot::GetState() { return m_state.load(); }
int Bot::GetRoomID() { return m_currentRoomID; }
const std::string& Bot::GetUserID() { return m_userID; }


// private methods
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
            std::this_thread::sleep_for(std::chrono::milliseconds(3000)); // wait before retrying
            continue;
        }

        // parse packets from recvBuffer
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
            Log("[" + m_userID + "] Login successful -> InLobby");
            m_state.store(BotState::InLobby);
        }
        else
        {
            Log("[" + m_userID + "] Login failed");
        }
        break;
    }
    case PacketType::CreateRoomRes:
    {
        PktCreateRoomRes* pRes = reinterpret_cast<PktCreateRoomRes*>(pPacketData);
        if (pRes->success)
        {
            Log("[" + m_userID + "] Room creation successful (Room " + std::to_string(pRes->newRoomID) + ") -> InRoom");
            m_currentRoomID.store(pRes->newRoomID);
            m_state.store(BotState::InRoom);
        }
        else
        {
            Log("[" + m_userID + "] Room creation failed");
        }
        break;
    }
    case PacketType::EnterRoomRes:
    {
        PktEnterRoomRes* pRes = reinterpret_cast<PktEnterRoomRes*>(pPacketData);
        if (pRes->success)
        {
            Log("[" + m_userID + "] Successfully entered the room (Room " + std::to_string(pRes->roomID) + ") -> InRoom");
            m_currentRoomID.store(pRes->roomID);
            m_state.store(BotState::InRoom);
        }
        else
        {
            Log("[" + m_userID + "] Failed to enter the room (no available rooms to enter)");
        }
        break;
    }
    case PacketType::LeaveRoomRes:
    {
        PktLeaveRoomRes* pRes = reinterpret_cast<PktLeaveRoomRes*>(pPacketData);
        if (pRes->success)
        {
            Log("[" + m_userID + "] Successfully left the room -> InLobby");
            m_currentRoomID.store(LOBBY_ID);
            m_state.store(BotState::InLobby);
        }
        break;
    }
    case PacketType::ChatNtf:
        /* intentional fallthrough */
    case PacketType::UserEnterNtf:
        /* intentional fallthrough */
    case PacketType::UserLeaveNtf:
        /* intentional fallthrough */
    case PacketType::UserListNtf:
        // bot ignores other user info and chat messages
        break;
    default:
        break;
    }
}

void Bot::BuildBehaviorTree()
{
    auto root = std::make_unique<Selector>();

    // 1. (Disconnected state) -> try to connect
    auto seqConnect = std::make_unique<Sequence>();
    seqConnect->AddChild(std::make_unique<Cond_IsState>(BotState::Disconnected));
    seqConnect->AddChild(std::make_unique<Act_TryConnect>());
    root->AddChild(std::move(seqConnect));

    // 2. (Cnnected state) -> try to login
    auto seqLogin = std::make_unique<Sequence>();
    seqLogin->AddChild(std::make_unique<Cond_IsState>(BotState::Connected));
    seqLogin->AddChild(std::make_unique<Act_SendLogin>());
    root->AddChild(std::move(seqLogin));

    // 3. (InLobby state) -> select lobby actions
    auto seqLobby = std::make_unique<Sequence>();
    seqLobby->AddChild(std::make_unique<Cond_IsState>(BotState::InLobby));
    seqLobby->AddChild(std::make_unique<Act_Wait>(1000, 3000));

    auto probLobby = std::make_unique<ProbabilisticSelector>();
    probLobby->AddChild(std::make_unique<Act_SendChat>(true), 70.0); // 70% chat in lobby
    probLobby->AddChild(std::make_unique<Act_SendCreateRoom>(), 15.0); // 15% create room
    probLobby->AddChild(std::make_unique<Act_SendEnterRandomRoom>(), 10.0); // 10% enter random room
    probLobby->AddChild(std::make_unique<Act_DoNothing>(), 5.0); // 5% do nothing

    seqLobby->AddChild(std::move(probLobby));
    root->AddChild(std::move(seqLobby));

    // 4. (InRoom state) -> select room actions
    auto seqRoom = std::make_unique<Sequence>();
    seqRoom->AddChild(std::make_unique<Cond_IsState>(BotState::InRoom));
    seqRoom->AddChild(std::make_unique<Act_Wait>(1000, 5000));

    auto probRoom = std::make_unique<ProbabilisticSelector>();
    probRoom->AddChild(std::make_unique<Act_SendChat>(false), 80.0); // 80% chat in room
    probRoom->AddChild(std::make_unique<Act_SendLeaveRoom>(), 15.0); // 15% leave room
    probRoom->AddChild(std::make_unique<Act_DoNothing>(), 5.0); // 5% do nothing

    seqRoom->AddChild(std::move(probRoom));
    root->AddChild(std::move(seqRoom));

    // 5. attach the tree to the bot
    m_behaviorTree = std::move(root);
}
