#ifndef FW_PCH_HPP
#define FW_PCH_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <atomic>
#include <chrono>
#include <cwctype>
#include <d3d11.h>
#include <format>
#include <fstream>
#include <functional>
#include <psapi.h>
#include <shellapi.h>
#include <string>
#include <thread>
#include <unordered_set>
#include <variant>
#include <vector>
#include <windows.h>

#include <deque>
#include <mutex>

using namespace std::chrono_literals;

#include <MinHook.h>
#include <Zydis/Zydis.h>
#include <nlohmann/json.hpp>

#endif // FW_PCH_HPP
