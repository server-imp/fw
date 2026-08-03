#ifndef FW_PATTERN_HPP
#define FW_PATTERN_HPP
#pragma once

#include "handle.hpp"
#include "pch.hpp"
#include "range.hpp"

namespace memory
{
    struct PatternTransform
    {
        enum class Type
        {
            Add,
            Subtract,
            Dereference,
            RipRelative,
            Match,
            RelCall,
            Callback
        };

        Type     type;
        intptr_t value;

        explicit PatternTransform(Type type, intptr_t value);

        [[nodiscard]] std::string toString() const;
    };

    class Pattern
    {
    public:
        using Callback = std::function<bool(Handle&)>;

    private:
        std::string _name {};

        std::string                       _ida {};
        std::vector<std::vector<uint8_t>> _data {};
        std::vector<std::vector<uint8_t>> _mask {};

        std::vector<PatternTransform> _transforms {};
        std::vector<Callback>         _callbacks {};

        Handle _result {};
        bool   _resolved {};

        static void parseIda(const std::string& ida, std::vector<uint8_t>& data, std::vector<uint8_t>& mask);

    public:
        explicit Pattern(std::string name, const std::string& ida);
        explicit Pattern(const std::string& ida);
        explicit Pattern(std::string name, const std::initializer_list<std::string>& idas);
        explicit Pattern(const std::initializer_list<std::string>& idas);

        [[nodiscard]] const std::string& name() const;

        [[nodiscard]] const Handle& result() const;

        [[nodiscard]] bool resolved() const;

        Pattern& add(ptrdiff_t offset);
        Pattern& subtract(ptrdiff_t offset);
        Pattern& dereference();
        Pattern& rip();
        Pattern& relCall();
        Pattern& match(intptr_t byte);
        Pattern& callback(const Callback& callback);

        bool resolve(const Range& range, Handle& result);
    };
} // namespace memory

#endif // FW_PATTERN_HPP
