#pragma once
#include "pch.h"

// IocpManager 이름 공간으로 모든 관련 함수를 묶습니다.
namespace IocpManager
{
    // --- IOCP 및 스레드 관리 함수 ---
    bool InitIocp(int threadCount);
    void StartWorkerThreads(int threadCount);
    void StartTimeoutThread();
    void StopIocp();

    HANDLE GetIocpHandle();

    // --- 세션 관리 함수 ---
    void AddSession(Session* pSession);
    void RemoveSession(SOCKET socket, Session* pSession);
}
