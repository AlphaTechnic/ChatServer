#pragma once

#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <random>
#include <chrono>
#include <numeric>
#include <functional>    // for std::hash

#include <winsock2.h>
#include <ws2tcpip.h>

#include "Protocol.h"

#pragma comment(lib, "ws2_32.lib")

// forward declarations to reduce header dependencies for pointer/reference usage
class Bot;
