#ifndef FW_RANGE_HPP
#define FW_RANGE_HPP
#pragma once
#include "handle.hpp"

namespace memory
{
    class Range
    {
    protected:
        Handle _start {};
        Handle _end {};
        size_t _size {};

    public:
        Range() = default;

        explicit Range(const char* moduleName);

        explicit Range(const Handle& start, size_t size);

        explicit Range(const Handle& start, const Handle& end);

        explicit Range(uintptr_t start, size_t size);

        [[nodiscard]] const Handle& start() const;

        [[nodiscard]] const Handle& end() const;

        [[nodiscard]] const size_t& size() const;

        [[nodiscard]] bool contains(const Handle& address) const;

        [[nodiscard]] bool contains(uintptr_t address) const;

        static bool contains(const std::vector<Range>& ranges, const Handle& address);

        static bool contains(const std::vector<Range>& ranges, uintptr_t address);

        bool operator<(const Range& other) const noexcept;

        bool operator==(const Range& other) const noexcept;
    };
}

#endif //FW_RANGE_HPP
