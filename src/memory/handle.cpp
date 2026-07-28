#include "handle.hpp"

#include "logger.hpp"
#include "protection.hpp"

memory::Handle::Handle(void* pointer)
{
    this->_pointer = reinterpret_cast<uintptr_t>(pointer);
}

memory::Handle::Handle(const uint64_t pointer)
{
    this->_pointer = pointer;
}

memory::Handle::Handle(const Handle& other)
{
    this->_pointer = other._pointer;
}

uintptr_t memory::Handle::raw() const
{
    return this->_pointer;
}

memory::Handle memory::Handle::add(const ptrdiff_t offset) const
{
    return Handle(this->_pointer + offset);
}

memory::Handle memory::Handle::add(const Handle& other) const
{
    return Handle(this->_pointer + other._pointer);
}

memory::Handle memory::Handle::sub(const ptrdiff_t offset) const
{
    return Handle(this->_pointer - offset);
}

memory::Handle memory::Handle::sub(const Handle& other) const
{
    return Handle(this->_pointer - other._pointer);
}

memory::Handle memory::Handle::rip() const
{
    return add(deref<int32_t>()).add(4);
}

memory::Handle memory::Handle::resolve_relative_call() const
{
    LOG_DBG("Resolving relative call at {}", formatted());

    if (const auto byte = deref<uint8_t>(); byte != 0xE8)
    {
        LOG_DBG("{:02X} != E8", byte);
        return {};
    }

    const auto offset = add(1).deref<int32_t>();
    LOG_DBG("Offset: {:X}", offset);

    const auto nextInstruction = add(5);
    LOG_DBG("Next instruction: {}", nextInstruction.formatted());

    auto result = nextInstruction.add(offset);
    LOG_DBG("Resolved to: {}", result.formatted());

    return result;
}

bool memory::Handle::nop(const size_t size) const
{
    LOG_DBG("NOPing {} bytes at {}", size, formatted());

    if (size == 0)
    {
        LOG_DBG("Invalid size");
        return false;
    }

    const auto address = reinterpret_cast<void*>(_pointer);
    Protection protection(*this, size, PAGE_EXECUTE_READWRITE);

    std::memset(address, 0x90, size);

    if (FlushInstructionCache(GetCurrentProcess(), address, size) == 0)
    {
        LOG_DBG("Failed to flush instruction cache");
        return false;
    }

    return true;
}

bool memory::Handle::null() const
{
    return _pointer == 0;
}

const std::string& memory::Handle::formatted() const
{
    return format(*this);
}

bool memory::Handle::operator==(const Handle& other) const noexcept
{
    return _pointer == other._pointer;
}

bool memory::Handle::operator==(const uintptr_t other) const noexcept
{
    return _pointer == other;
}

bool memory::Handle::operator!=(const uintptr_t other) const noexcept
{
    return _pointer != other;
}

bool memory::Handle::operator!=(const Handle& other) const noexcept
{
    return _pointer != other._pointer;
}

bool memory::Handle::operator<(const Handle& other) const noexcept
{
    return _pointer < other._pointer;
}

bool memory::Handle::operator<(const uintptr_t other) const noexcept
{
    return _pointer < other;
}

bool memory::Handle::operator<=(const Handle& other) const noexcept
{
    return _pointer <= other._pointer;
}

bool memory::Handle::operator<=(const uintptr_t other) const noexcept
{
    return _pointer <= other;
}

bool memory::Handle::operator>(const Handle& other) const noexcept
{
    return _pointer > other._pointer;
}

bool memory::Handle::operator>(const uintptr_t other) const noexcept
{
    return _pointer > other;
}

bool memory::Handle::operator>=(const Handle& other) const noexcept
{
    return _pointer >= other._pointer;
}

bool memory::Handle::operator>=(const uintptr_t other) const noexcept
{
    return _pointer >= other;
}

static std::unordered_map<uintptr_t, std::string> formattedHandles;

const std::string& memory::Handle::format(const Handle& handle)
{
    if (const auto find = formattedHandles.find(handle.raw()); find != formattedHandles.end())
    {
        return find->second;
    }

    std::string result;
    if (Module module {}; Module::tryGetByAddr(handle, module))
    {
        result = std::format("{}+{:X}", module.name(), handle.sub(module.start()).raw());
    } else
    {
        result = std::format("{:08X}", handle.raw());
    }

    return formattedHandles.emplace(handle.raw(), result).first->second;
}
