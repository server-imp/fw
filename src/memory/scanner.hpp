#ifndef FW_SCANNER_HPP
#define FW_SCANNER_HPP
#pragma once

#include "range.hpp"

namespace memory
{
    class Scanner
    {
    public:
        Scanner() = delete;

        ~Scanner() = delete;

        Scanner& operator=(const Scanner&) = delete;

        static bool findPattern(const std::string& pattern, const Range& range, Handle& result);

        static bool findString(const std::string& string, const Range& range, Handle& result);

        static bool findWstring(const std::wstring& string, const Range& range, Handle& result);

        static bool findWstringReference(const std::wstring& string, Handle& result);
    };
}

#endif //FW_SCANNER_HPP
