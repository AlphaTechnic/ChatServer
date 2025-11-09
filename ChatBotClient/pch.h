#pragma once

// --- C++ Standard Library ---
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <memory>        // for std::unique_ptr
#include <atomic>        // for std::atomic
#include <mutex>         // for std::mutex
#include <random>        // for std::mt19937
#include <chrono>        // for std::this_thread::sleep_for
#include <numeric>       // for std::iota
#include <functional>    // for std::hash

// --- Windows API ---
#include <winsock2.h>
#include <ws2tcpip.h>

// --- Project Headers ---
#include "Protocol.h" // 공통 프로토콜 헤더

// --- Winsock Lib ---
#pragma comment(lib, "ws2_32.lib")

// --- 전방 선언 ---
class Bot;
