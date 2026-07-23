#ifndef FW_MEMORY_HPP
#define FW_MEMORY_HPP
#pragma once

#include "handle.hpp"
#include "hook.hpp"
#include "range.hpp"
#include "pattern.hpp"
#include "scanner.hpp"
#include "module.hpp"
#include "pointer_validator.hpp"
#include "protection.hpp"
#include "patch.hpp"

namespace memory
{
    bool tryNearAlloc(const Handle& target, size_t size, Handle& result);

    bool locateAllPointers(
        const Handle&        base,
        size_t               largestOffset,
        const Handle&        target,
        std::vector<Handle>& results
    );

    // hacky way to find the start of a function if it is preceded by at least two int3's or an int3 and a ret
    bool findFunctionStart(const Handle& instruction, Handle& result);
}

#endif //FW_MEMORY_HPP
