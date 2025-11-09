#pragma once

// --- C++ Standard Library ---
#include <iostream>
#include <thread>
#include <string>
#include <atomic> // (NetworkClient에서 g_isConnected 용)

// --- Windows API ---
#include <winsock2.h>
#include <ws2tcpip.h>

// --- Project Headers ---
#include "Protocol.h" // Common 프로젝트의 헤더

// --- Winsock Lib ---
#pragma comment(lib, "ws2_32.lib")