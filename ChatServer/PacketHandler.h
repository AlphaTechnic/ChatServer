#pragma once
#include "pch.h"

// asynchronous send
void PostSend(Session* pSession, char* pPacket, int size);

// handle completed packets
void ProcessPacket(Session* pSession, char* pPacketData);

// parse received data and assemble/process packets
void ProcessRecv(Session* pSession, DWORD bytesTransferred);
