#ifndef FW_PCH_HPP
#define FW_PCH_HPP

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <shellapi.h>
#include <functional>
#include <variant>
#include <psapi.h>
#include <fstream>
#include <d3d11.h>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <deque>
#include <mutex>

using namespace std::chrono_literals;

#include <MinHook.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <Zydis/Zydis.h>

#endif //FW_PCH_HPP
