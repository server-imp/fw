#include "pattern.hpp"

#include <utility>

#include "range.hpp"
#include "scanner.hpp"
#include "../logger.hpp"
#include "util.hpp"

memory::PatternTransform::PatternTransform(
    const PatternTransformType type,
    const intptr_t             value
) : type(type), value(value) {}

std::string memory::PatternTransform::toString() const
{
    auto typeString = "UNKNOWN";

    switch (this->type)
    {
    case PatternTransformType::Add: typeString = "Add";
        break;
    case PatternTransformType::Subtract: typeString = "Subtract";
        break;
    case PatternTransformType::Dereference: typeString = "Dereference";
        break;
    case PatternTransformType::RipRelative: typeString = "RipRelative";
        break;
    case PatternTransformType::Match: typeString = "Match";
        break;
    case PatternTransformType::Callback: typeString = "Callback";
        break;
    }

    return fmt::format("{} [{:X}]", typeString, value);
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

memory::Pattern::Pattern(std::string name, const std::string& ida)
    : _name(std::move(name)), _ida(ida)
{
    auto& data = _data.emplace_back();
    auto& mask = _mask.emplace_back();
    parseIda(ida, data, mask);
}

memory::Pattern::Pattern(const std::string& ida)
    : Pattern("", ida) {}

memory::Pattern::Pattern(std::string name, const std::initializer_list<std::string>& idas)
    : _name(std::move(name))
{
    for (const auto& ida : idas)
    {
        auto& data = _data.emplace_back();
        auto& mask = _mask.emplace_back();
        parseIda(ida, data, mask);
    }
}

memory::Pattern::Pattern(const std::initializer_list<std::string>& idas)
    : Pattern("", idas) {}

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
    _transforms.emplace_back(PatternTransformType::Add, offset);
    return *this;
}

memory::Pattern& memory::Pattern::subtract(ptrdiff_t offset)
{
    _transforms.emplace_back(PatternTransformType::Subtract, offset);
    return *this;
}

memory::Pattern& memory::Pattern::dereference()
{
    _transforms.emplace_back(PatternTransformType::Dereference, 0);
    return *this;
}

memory::Pattern& memory::Pattern::rip()
{
    _transforms.emplace_back(PatternTransformType::RipRelative, 0);
    return *this;
}

memory::Pattern& memory::Pattern::match(intptr_t byte)
{
    _transforms.emplace_back(PatternTransformType::Match, byte);
    return *this;
}

memory::Pattern& memory::Pattern::callback(const Callback& callback)
{
    _transforms.emplace_back(PatternTransformType::Callback, _callbacks.size());
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
    LOG_DBG("Locating pattern");
    for (size_t i = 0; i < _data.size(); ++i)
    {
        if (!Scanner::findPattern(range, _data[i], _mask[i], pointer))
        {
            continue;
        }

        LOG_DBG("Found pattern {} at {:08X}", i + 1, pointer.raw());
        break;
    }

    if (pointer.null())
    {
        LOG_WARN("Could not find pattern");
        return false;
    }

    for (size_t i = 0; i < _transforms.size(); ++i)
    {
        const auto& transform = _transforms[i];

        if (transform.type == PatternTransformType::Add)
        {
            pointer = pointer.add(transform.value);
            continue;
        }

        if (transform.type == PatternTransformType::Subtract)
        {
            pointer = pointer.sub(transform.value);
            continue;
        }

        if (transform.type == PatternTransformType::Dereference)
        {
            pointer = Handle(*pointer.to_ptr<uintptr_t*>());
            continue;
        }

        if (transform.type == PatternTransformType::RipRelative)
        {
            pointer = pointer.rip();
            continue;
        }

        if (transform.type == PatternTransformType::Match)
        {
            const auto byte  = pointer.deref<uint8_t>();
            const auto check = static_cast<uint8_t>(transform.value);
            if (byte != check)
            {
                LOG_WARN(
                    "Transform number {} \"{}\" failed: {:02X} != {:02X}",
                    i + 1,
                    transform.toString(),
                    byte,
                    check
                );
                return false;
            }
        }

        if (transform.type == PatternTransformType::Callback)
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

    LOG_INFO("Resolved \"{}\" => {:08X}", _name, _result.raw());
    return true;
}
