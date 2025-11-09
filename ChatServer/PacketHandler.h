#pragma once
#include "pch.h"

/**
 * @brief 비동기 Send 요청
 */
void PostSend(Session* pSession, char* pPacket, int size);

/**
 * @brief 완성된 패킷을 처리하는 함수
 */
void ProcessPacket(Session* pSession, char* pPacketData);

/**
 * @brief 수신된 데이터를 파싱하고 패킷을 조립/처리하는 함수
 */
void ProcessRecv(Session* pSession, DWORD bytesTransferred);
