#ifndef FW_PROXY_HPP
#define FW_PROXY_HPP
#pragma once

#include "pch.hpp"

namespace proxy
{
    bool check(const std::initializer_list<std::string>& candidates, std::string& proxyName);
}

#endif //FW_PROXY_HPP
