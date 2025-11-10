#pragma once
#include "pch.h"

void PostSend(Session* pSession, char* pPacket, int size);
void ProcessPacket(Session* pSession, char* pPacketData);
void ProcessRecv(Session* pSession, DWORD bytesTransferred);
