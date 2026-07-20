#ifndef FW_PCH_HPP
#define FW_PCH_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <functional>
#include <filesystem>
#include <algorithm>
#include <psapi.h>
#include <thread>
#include <chrono>
#include <vector>
#include <sstream>
#include <string>
#include <cstdio>
#include <optional>
#include <atomic>
#include <deque>
#include <fstream>
#include <mutex>
#include <iostream>
#include <codecvt>
#include <set>
#include <map>
#include <unordered_set>
using namespace std::chrono_literals;

#include <MinHook.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <Zydis/Zydis.h>

#endif //FW_PCH_HPP
