#include "pattern.hpp"

#include <utility>

#include "../logger.hpp"
#include "range.hpp"
#include "scanner.hpp"
#include "util.hpp"

memory::PatternTransform::PatternTransform(const Type type, const intptr_t value) : type(type), value(value) {}

std::string memory::PatternTransform::toString() const
{
    auto typeString = "UNKNOWN";

    switch (this->type)
    {
    case Type::Add:
        typeString = "Add";
        break;
    case Type::Subtract:
        typeString = "Subtract";
        break;
    case Type::Dereference:
        typeString = "Dereference";
        break;
    case Type::RipRelative:
        typeString = "RipRelative";
        break;
    case Type::Match:
        typeString = "Match";
        break;
    case Type::RelCall:
        typeString = "RelCall";
        break;
    case Type::Callback:
        typeString = "Callback";
        break;
    }

    return std::format("{} [{:X}]", typeString, value);
}

void memory::Pattern::parseIda(const std::string& ida, std::vector<uint8_t>& data, std::vector<uint8_t>& mask)
{
    std::istringstream ss(ida);
    std::string        token;

    while (ss >> token)
    {
        if (token == "??" || token == "?")
        {
            data.push_back(0x00);
            mask.push_back(0x00);
        }
        else
        {
            data.push_back(static_cast<uint8_t>(std::stoul(token, nullptr, 16)));
            mask.push_back(0xFF);
        }
    }
}

memory::Pattern::Pattern(std::string name, const std::string& ida) : _name(std::move(name)), _ida(ida)
{
    auto& data = _data.emplace_back();
    auto& mask = _mask.emplace_back();
    parseIda(ida, data, mask);
}

memory::Pattern::Pattern(const std::string& ida) : Pattern("", ida) {}

memory::Pattern::Pattern(std::string name, const std::initializer_list<std::string>& idas) : _name(std::move(name))
{
    for (const auto& ida : idas)
    {
        auto& data = _data.emplace_back();
        auto& mask = _mask.emplace_back();
        parseIda(ida, data, mask);
    }
}

memory::Pattern::Pattern(const std::initializer_list<std::string>& idas) : Pattern("", idas) {}

const std::string& memory::Pattern::name() const
{
    return _name;
}

const memory::Handle& memory::Pattern::result() const
{
    return _result;
}

bool memory::Pattern::resolved() const
{
    return _resolved;
}

memory::Pattern& memory::Pattern::add(ptrdiff_t offset)
{
    _transforms.emplace_back(PatternTransform::Type::Add, offset);
    return *this;
}

memory::Pattern& memory::Pattern::subtract(ptrdiff_t offset)
{
    _transforms.emplace_back(PatternTransform::Type::Subtract, offset);
    return *this;
}

memory::Pattern& memory::Pattern::dereference()
{
    _transforms.emplace_back(PatternTransform::Type::Dereference, 0);
    return *this;
}

memory::Pattern& memory::Pattern::rip()
{
    _transforms.emplace_back(PatternTransform::Type::RipRelative, 0);
    return *this;
}

memory::Pattern& memory::Pattern::relCall()
{
    _transforms.emplace_back(PatternTransform::Type::RelCall, 0);
    return *this;
}

memory::Pattern& memory::Pattern::match(intptr_t byte)
{
    _transforms.emplace_back(PatternTransform::Type::Match, byte);
    return *this;
}

memory::Pattern& memory::Pattern::callback(const Callback& callback)
{
    _transforms.emplace_back(PatternTransform::Type::Callback, _callbacks.size());
    _callbacks.emplace_back(callback);
    return *this;
}

bool memory::Pattern::resolve(const Range& range, Handle& result)
{
    LOG_INFO("Resolving pattern: \"{}\"", _name.c_str());

    if (_resolved)
    {
        LOG_WARN("Already resolved");
        result = _result;
        return true;
    }

    Handle pointer {};
    for (size_t i = 0; i < _data.size(); ++i)
    {
        if (!Scanner::findPattern(range, _data[i], _mask[i], pointer))
        {
            continue;
        }
        break;
    }

    if (pointer.null())
    {
        return false;
    }

    for (size_t i = 0; i < _transforms.size(); ++i)
    {
        const auto& transform = _transforms[i];

        if (transform.type == PatternTransform::Type::Add)
        {
            pointer = pointer.add(transform.value);
            continue;
        }

        if (transform.type == PatternTransform::Type::Subtract)
        {
            pointer = pointer.sub(transform.value);
            continue;
        }

        if (transform.type == PatternTransform::Type::Dereference)
        {
            pointer = Handle(*pointer.to_ptr<uintptr_t*>());
            continue;
        }

        if (transform.type == PatternTransform::Type::RipRelative)
        {
            pointer = pointer.rip();
            continue;
        }

        if (transform.type == PatternTransform::Type::Match)
        {
            const auto byte  = pointer.deref<uint8_t>();
            const auto check = static_cast<uint8_t>(transform.value);
            if (byte != check)
            {
                LOG_WARN(
                    "Transform number {} \"{}\" failed: {:02X} != {:02X}", i + 1, transform.toString(), byte, check);
                return false;
            }
        }

        if (transform.type == PatternTransform::Type::RelCall)
        {
            if (pointer.deref<uint8_t>() != 0xE8)
            {
                LOG_WARN("Transform number {} \"{}\" failed: {:02X} != 0xE8",
                         i + 1,
                         transform.toString(),
                         pointer.deref<uint8_t>());
                return false;
            }

            pointer = pointer.resolve_relative_call();
            continue;
        }

        if (transform.type == PatternTransform::Type::Callback)
        {
            if (!_callbacks[transform.value](pointer))
            {
                LOG_WARN("Transform number {} \"{}\" failed", i + 1, transform.toString());
                return false;
            }
            continue;
        }
    }

    _result   = pointer;
    result    = _result;
    _resolved = true;

    LOG_INFO("Resolved \"{}\" => {}", _name, _result.formatted());
    return true;
}
