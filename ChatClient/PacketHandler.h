#pragma once
#include "pch.h"

// 서버로부터 받은 패킷을 처리하는 함수들의 네임스페이스
namespace PacketHandler
{
	/**
	 * @brief 서버로부터 받은 완성된 패킷을 처리합니다.
	 * @details 이 함수는 Recv 스레드에서 호출됩니다.
	 */
	void ProcessPacket(char* pPacketData);
}