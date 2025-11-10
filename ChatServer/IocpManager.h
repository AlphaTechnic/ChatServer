#pragma once
#include "pch.h"

namespace IocpManager
{
    // functions for IOCP management
    bool InitIocp(int threadCount);
    void StartWorkerThreads(int threadCount);
    void StartTimeoutThread();
    void StopIocp();

    HANDLE GetIocpHandle();

    // functions for session management
    void AddSession(Session* pSession);
    void RemoveSession(SOCKET socket, Session* pSession);
}
