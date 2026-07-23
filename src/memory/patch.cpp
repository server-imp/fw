#include "patch.hpp"

#include "logger.hpp"
#include "protection.hpp"

memory::Patch::Patch(std::string name) : _name(std::move(name)) {}

bool memory::Patch::enabled() const
{
    return _enabled;
}

bool memory::Patch::valid() const
{
    return true;
}

bool memory::Patch::enable()
{
    return false;
}

bool memory::Patch::disable()
{
    return false;
}

memory::BytePatch::BytePatch(
    const std::string&                    name,
    const Handle&                         target,
    const bool                            flushInstructionCache,
    const std::initializer_list<uint8_t>& patchBytes
) : Patch(name), _target(target), _patched(patchBytes), _flushInstructionCache(flushInstructionCache) {}

bool memory::BytePatch::enable()
{
    LOG_DBG("Enabling patch \"{}\"", _name);

    if (_enabled)
    {
        LOG_DBG("Already enabled");
        return true;
    }

    if (_patched.empty())
    {
        LOG_DBG("Empty patch");
        return false;
    }

    Protection protection(_target, _patched.size(), PAGE_EXECUTE_READWRITE);

    if (!protection.success())
    {
        LOG_ERR("Failed to protect memory");
        return false;
    }

    const auto target = _target.to_ptr<uint8_t*>();

    if (_original.empty())
    {
        LOG_DBG("Backing up original bytes");
        _original.resize(_patched.size());
        std::memcpy(_original.data(), target, _patched.size());
    }

    std::memcpy(target, _patched.data(), _patched.size());

    if (_flushInstructionCache && FlushInstructionCache(GetCurrentProcess(), target, _patched.size()) == 0)
    {
        LOG_DBG("Failed to flush instruction cache");
    }

    _enabled = true;
    LOG_DBG("Enabled");
    return true;
}

bool memory::BytePatch::disable()
{
    LOG_DBG("Disabling patch \"{}\"", _name);

    if (!_enabled)
    {
        LOG_DBG("Already disabled");
        return true;
    }

    Protection protection(_target, _patched.size(), PAGE_EXECUTE_READWRITE);
    if (!protection.success())
    {
        LOG_ERR("Failed to protect memory");
        return false;
    }

    const auto target = _target.to_ptr<uint8_t*>();
    std::memcpy(target, _original.data(), _patched.size());

    if (_flushInstructionCache && FlushInstructionCache(GetCurrentProcess(), target, _patched.size()) == 0)
    {
        LOG_DBG("Failed to flush instruction cache");
    }

    _enabled = false;
    LOG_DBG("Disabled");
    return true;
}

std::shared_ptr<memory::BytePatch> memory::BytePatch::create(
    const std::string&                    name,
    const Handle&                         target,
    bool                                  flushInstructionCache,
    const std::initializer_list<uint8_t>& patchBytes
)
{
    return std::make_shared<BytePatch>(name, target, flushInstructionCache, patchBytes);
}

memory::NopPatch::NopPatch(const std::string& name, const Handle& target, const size_t size)
    : BytePatch(name, target, true, {})
{
    _patched = std::vector<uint8_t>(size, 0x90);
}

bool memory::NopPatch::enable()
{
    return BytePatch::enable();
}

bool memory::NopPatch::disable()
{
    return BytePatch::disable();
}

std::shared_ptr<memory::NopPatch> memory::NopPatch::create(const std::string& name, const Handle& target, size_t size)
{
    return std::make_shared<NopPatch>(name, target, size);
}

memory::RefNopPatch::RefNopPatch(std::string name, Module& module, const Handle& target, const RefData::Type refType)
    : Patch(std::move(name))
{
    std::vector<RefData> refs {};
    if (!module.findReferences(target, refs, refType))
    {
        LOG_ERR("Failed to find references");
        return;
    }

    _patches.reserve(refs.size());
    size_t count = 1;
    for (const auto& ref : refs)
    {
        _patches.push_back(
            NopPatch::create(fmt::format("{}_{}", _name, count), ref.instruction(), ref.instructionLength())
        );
        ++count;
    }
}

bool memory::RefNopPatch::enable()
{
    if (_enabled)
    {
        LOG_DBG("Already enabled");
        return true;
    }

    for (const auto& patch : _patches)
    {
        if (!patch->enable())
        {
            goto disable;
        }
    }

    _enabled = true;
    LOG_DBG("Enabled");
    return true;
disable:
    for (const auto& patch : _patches)
    {
        if (patch->enabled())
            patch->disable();
    }
    return false;
}

bool memory::RefNopPatch::disable()
{
    if (!_enabled)
    {
        LOG_DBG("Already disabled");
        return true;
    }

    for (const auto& patch : _patches)
    {
        if (!patch->disable())
        {
            goto enable;
        }
    }

    _enabled = false;
    LOG_DBG("Disabled");
    return true;
enable:
    for (const auto& patch : _patches)
    {
        if (!patch->enabled())
            patch->enable();
    }
    return false;
}

std::shared_ptr<memory::RefNopPatch> memory::RefNopPatch::create(
    const std::string& name,
    Module&            module,
    const Handle&      target,
    RefData::Type      refType
)
{
    return std::make_shared<RefNopPatch>(name, module, target, refType);
}

memory::StringRefPatch::StringRefPatch(std::string name, const RefData& ref) : Patch(std::move(name))
{
    _lea            = ref.instruction();
    _originalString = ref.reference();
}

void memory::StringRefPatch::setString(const std::string& string)
{
    const bool wasEnabled = _enabled;

    if (_enabled && !disable())
    {
        LOG_ERR("Failed to disable patch");
        return;
    }

    const auto size = string.size() + 1;

    if (_allocationSize < size)
    {
        if (_allocationSize > 0)
        {
            VirtualFree(_allocation.to_ptr<void*>(), _allocationSize, MEM_RELEASE);
            _allocationSize = 0;
        }

        if (!tryNearAlloc(_lea, size, _allocation))
        {
            LOG_ERR("Failed to allocate memory");
            return;
        }

        _allocationSize = size;
    }

    auto* ptr = _allocation.to_ptr<char*>();
    std::memcpy(ptr, string.data(), string.size());
    ptr[string.size()] = '\0';

    LOG_DBG("Set string to \"{}\"", string);

    if (wasEnabled && !enable())
    {
        LOG_ERR("Failed to re-enable patch");
    }
}

void memory::StringRefPatch::setWstring(const std::wstring& string)
{
    const bool wasEnabled = _enabled;

    if (_enabled && !disable())
    {
        LOG_ERR("Failed to disable patch");
        return;
    }

    const auto size = (string.size() + 1) * sizeof(wchar_t);

    if (_allocationSize < size)
    {
        if (_allocationSize > 0)
        {
            VirtualFree(_allocation.to_ptr<void*>(), _allocationSize, MEM_RELEASE);
            _allocationSize = 0;
        }

        if (!tryNearAlloc(_lea, size, _allocation))
        {
            LOG_ERR("Failed to allocate memory");
            return;
        }

        _allocationSize = size;
    }

    auto* ptr = _allocation.to_ptr<wchar_t*>();
    std::memcpy(ptr, string.data(), string.size() * sizeof(wchar_t));
    ptr[string.size()] = L'\0';

    LOG_DBG("Set wstring to \"{}\"", util::wstringToString(string));

    if (wasEnabled && !enable())
    {
        LOG_ERR("Failed to re-enable patch");
    }
}

bool memory::StringRefPatch::enable()
{
    LOG_DBG("Enabling patch \"{}\"", _name);

    if (_enabled)
    {
        LOG_DBG("Already enabled");
        return true;
    }

    if (_allocation.raw() == 0 || _allocationSize == 0)
    {
        LOG_DBG("Invalid allocation (did you forget to set the string?)");
        return false;
    }

    Protection protection(_lea, 7, PAGE_EXECUTE_READWRITE);
    if (!protection.success())
    {
        LOG_ERR("Failed to protect memory");
        return false;
    }

    *_lea.add(3).to_ptr<int32_t*>() = static_cast<int32_t>(_allocation.raw() - _lea.add(7).raw());

    _enabled = true;
    LOG_DBG("Enabled");
    return true;
}

bool memory::StringRefPatch::disable()
{
    LOG_DBG("Disabling patch \"{}\"", _name);

    if (!_enabled)
    {
        LOG_DBG("Already disabled");
        return true;
    }

    Protection protection(_lea, 7, PAGE_EXECUTE_READWRITE);
    if (!protection.success())
    {
        LOG_ERR("Failed to protect memory");
        return false;
    }

    *_lea.add(3).to_ptr<int32_t*>() = static_cast<int32_t>(_originalString.raw() - _lea.add(7).raw());

    _enabled = false;
    LOG_DBG("Disabled");
    return true;
}

std::shared_ptr<memory::StringRefPatch> memory::StringRefPatch::create(const std::string& name, const RefData& lea)
{
    return std::make_shared<StringRefPatch>(name, lea);
}
