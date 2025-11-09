#include "pch.h"
#include "IocpManager.h" // IOCP, 스레드, 세션 관리
#include "Session.h"

int main()
{
    // 1. Winsock 초기화
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed!" << std::endl;
        return 1;
    }

    // 2. IOCP 포트 생성
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    int threadCount = sysInfo.dwNumberOfProcessors;

    if (!IocpManager::InitIocp(threadCount))
    {
        WSACleanup();
        return 1;
    }

    // 3. 워커 스레드 및 타임아웃 스레드 시작
    IocpManager::StartWorkerThreads(threadCount);
    IocpManager::StartTimeoutThread();


    // 4. 리슨 소켓 생성
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET)
    {
        std::cerr << "Listen socket creation failed!" << std::endl;
        WSACleanup();
        return 1;
    }

    // 5. 서버 주소 설정 및 바인딩
    sockaddr_in serverAddr = { 0, };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(SERVER_PORT); // Protocol.h

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cerr << "Socket bind failed!" << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    // 6. 리슨 시작
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cerr << "Socket listen failed!" << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server :: Chat server listening on port " << SERVER_PORT << "..." << std::endl;

    // 7. 메인 스레드의 Accept 루프
    while (true)
    {
        sockaddr_in clientAddr = { 0, };
        int addrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &addrLen);

        if (clientSocket == INVALID_SOCKET)
        {
            std::cerr << "Accept failed!" << std::endl;
            // (서버 종료 시 listenSocket을 닫으면 accept가 실패하며 루프 탈출 가능)
            // TODO: 정상적인 서버 종료 로직 구현
            continue;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, INET_ADDRSTRLEN);
        std::cout << "Client connected from " << clientIp << ":" << ntohs(clientAddr.sin_port) << std::endl;

        // 1. 세션 생성
        Session* pSession = new Session(clientSocket);

        // 2. 세션 매니저에 추가
        IocpManager::AddSession(pSession);

        // 3. IOCP에 연결 (CompletionKey로 pSession 전달)
        CreateIoCompletionPort((HANDLE)clientSocket, IocpManager::GetIocpHandle(), (ULONG_PTR)pSession, 0);

        // 4. 첫 번째 비동기 수신(WSARecv) 요청
        DWORD recvBytes = 0;
        DWORD flags = 0;
        int recvResult = WSARecv(
            pSession->socket,
            &(pSession->recvOverlapped.wsaBuf),
            1,
            &recvBytes,
            &flags,
            &(pSession->recvOverlapped.overlapped),
            NULL
        );

        if (recvResult == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
        {
            std::cerr << "WSARecv failed immediately!" << std::endl;

            // 실패 시 즉시 정리
            IocpManager::RemoveSession(clientSocket, pSession);
        }
    }

    // 8. 정리
    IocpManager::StopIocp();
    closesocket(listenSocket);
    WSACleanup();
    return 0;
}
