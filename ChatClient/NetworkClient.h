#pragma once
#include "pch.h"

namespace NetworkClient
{
	bool ConnectToServer(const char* serverIP, short serverPort);
	void Disconnect();

    // send packet to server
	void SendPacket(char* pPacket, int size);
    // start the receive thread to get packets from server
	void StartRecvThread();
	bool IsConnected();
}
