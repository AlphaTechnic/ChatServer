#pragma once
#include "pch.h"

namespace IocpManager
{
    // IOCP managements
    bool InitIocp(int threadCount);
    void StartWorkerThreads(int threadCount);
    void StartTimeoutThread();
    void StopIocp();

    HANDLE GetIocpHandle();

    // session managements
    void AddSession(Session* pSession);
    void RemoveSession(SOCKET socket, Session* pSession);
}
