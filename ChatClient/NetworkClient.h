#pragma once
#include "pch.h"

// 네트워크 연결, 송/수신을 관리하는 네임스페이스
namespace NetworkClient
{
	/**
	 * @brief 서버에 접속을 시도합니다.
	 * @param serverIP (Protocol.h에 정의된) 서버 IP
	 * @param serverPort (Protocol.h에 정의된) 서버 Port
	 * @return 접속 성공 시 true, 실패 시 false
	 */
	bool ConnectToServer(const char* serverIP, short serverPort);

	/**
	 * @brief 서버와 접속을 종료합니다.
	 */
	void Disconnect();

	/**
	 * @brief 서버로 패킷을 전송합니다.
	 */
	void SendPacket(char* pPacket, int size);

	/**
	 * @brief 서버로부터 패킷을 수신하는 스레드를 시작합니다.
	 */
	void StartRecvThread();

	/**
	 * @brief 현재 서버에 접속 중인지 확인합니다.
	 */
	bool IsConnected();
}