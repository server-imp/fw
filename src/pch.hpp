#ifndef FW_PCH_HPP
#define FW_PCH_HPP

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <thread>
#include <atomic>
#include <deque>
#include <fstream>
#include <mutex>
#include <variant>
#include <shellapi.h>
#include <d3d11.h>
using namespace std::chrono_literals;

#include <MinHook.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <Zydis/Zydis.h>

#endif //FW_PCH_HPP
