#ifndef FW_MEMORY_HPP
#define FW_MEMORY_HPP
#pragma once

#include "handle.hpp"
#include "hook.hpp"
#include "module.hpp"
#include "patch.hpp"
#include "pattern.hpp"
#include "pointer_validator.hpp"
#include "protection.hpp"
#include "range.hpp"
#include "scanner.hpp"

namespace memory
{
    bool tryNearAlloc(const Handle& target, size_t size, Handle& result);

    bool
    locateAllPointers(const Handle& base, size_t largestOffset, const Handle& target, std::vector<Handle>& results);
} // namespace memory

#endif // FW_MEMORY_HPP
