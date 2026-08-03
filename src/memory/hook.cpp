#include <utility>

#include "hook.hpp"

#include "../logger.hpp"
#include "handle.hpp"
#include "util.hpp"

memory::Hook::Hook(std::string name, void* target, void* original, void* ownFunction)
    : Toggleable(std::move(name)), _target(target), _original(original), _ownFunction(ownFunction)
{
    const auto from = Handle(_target);
    const auto to   = Handle(_ownFunction);

    LOG_DBG("Created hook \"{}\" {} -> {}", this->name(), from.formatted(), to.formatted());
}

void* memory::Hook::target() const
{
    return _target;
}

memory::Detour::Detour(std::string name, void* target, void* ownFunction)
    : Hook(std::move(name), target, nullptr, ownFunction)
{
}

memory::Detour::~Detour()
{
    if (enabled())
    {
        disable();
    }
}

bool memory::Detour::internalEnable()
{
    auto status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
    {
        LOG_DBG("MH_Initialize failed: {}", MH_StatusToString(status));
        return false;
    }

    status = MH_CreateHook(_target, _ownFunction, &_original);
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED)
    {
        LOG_DBG("MH_CreateHook failed: {}", MH_StatusToString(status));
        return false;
    }

    status = MH_EnableHook(_target);
    if (status != MH_OK && status != MH_ERROR_ENABLED)
    {
        LOG_DBG("MH_EnableHook failed: {}", MH_StatusToString(status));
        return false;
    }

    return true;
}

bool memory::Detour::internalDisable()
{
    auto status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
    {
        LOG_DBG("MH_Initialize failed: {}", MH_StatusToString(status));
        return false;
    }

    status = MH_DisableHook(_target);
    if (status != MH_OK && status != MH_ERROR_DISABLED && status != MH_ERROR_NOT_CREATED)
    {
        LOG_DBG("MH_DisableHook failed: {}", MH_StatusToString(status));
        return false;
    }

    return true;
}

memory::HookScope::HookScope(std::atomic_uint32_t& counter) : counter(counter)
{
    counter.fetch_add(1, std::memory_order_acq_rel);
}

memory::HookScope::~HookScope()
{
    counter.fetch_sub(1, std::memory_order_acq_rel);
}
