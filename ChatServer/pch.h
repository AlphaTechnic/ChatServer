#pragma once

// --- C++ Standard Library ---
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <string>
#include <atomic>
#include <memory>
#include <random>
#include <chrono>
#include <deque>

// --- Windows API ---
#include <winsock2.h>
#include <ws2tcpip.h>

// --- Project Headers ---
#include "Protocol.h" // Common 프로젝트의 헤더

// --- Winsock Lib ---
#pragma comment(lib, "ws2_32.lib")

// --- 전방 선언 (Forward Declarations) ---
// 헤더 간의 의존성을 낮추기 위해 포인터/참조로만 사용하는 타입은 전방 선언합니다.
struct Session;
class Room;
class Lobby;
class RoomManager;
