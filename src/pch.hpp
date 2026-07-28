#ifndef FW_PCH_HPP
#define FW_PCH_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shellapi.h>
#include <functional>
#include <unordered_set>
#include <variant>
#include <psapi.h>
#include <fstream>
#include <d3d11.h>
#include <format>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <string>
#include <deque>
#include <mutex>

using namespace std::chrono_literals;

#include <MinHook.h>
#include <nlohmann/json.hpp>
#include <Zydis/Zydis.h>

#endif //FW_PCH_HPP
