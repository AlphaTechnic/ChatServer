#pragma once

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

#include <winsock2.h>
#include <ws2tcpip.h>

#include "Protocol.h"

#pragma comment(lib, "ws2_32.lib")

// forward declarations to reduce header dependencies for pointer/reference usage
struct Session;
class Room;
class Lobby;
class RoomManager;
