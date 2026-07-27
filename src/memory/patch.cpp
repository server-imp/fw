#include "patch.hpp"

#include "logger.hpp"
#include "protection.hpp"

bool memory::BytePatch::internalEnable()
{
    if (_patched.empty())
    {
        LOG_ERR("Empty patch");
        return false;
    }

    const Protection protection(_target, _patched.size(), PAGE_EXECUTE_READWRITE);

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

    if (_flushInstructionCache&& FlushInstructionCache(GetCurrentProcess(), target, _patched.size()) == 0)
    {
        LOG_WARN("Failed to flush instruction cache");
    }

    return true;
}

bool memory::BytePatch::internalDisable()
{
    const Protection protection(_target, _patched.size(), PAGE_EXECUTE_READWRITE);
    if (!protection.success())
    {
        LOG_ERR("Failed to protect memory");
        return false;
    }

    const auto target = _target.to_ptr<uint8_t*>();
    std::memcpy(target, _original.data(), _patched.size());

    if (_flushInstructionCache&& FlushInstructionCache(GetCurrentProcess(), target, _patched.size()) == 0)
    {
        LOG_WARN("Failed to flush instruction cache");
    }

    return true;
}

memory::BytePatch::BytePatch(
    const std::string&                    name,
    const Handle&                         target,
    const bool                            flushInstructionCache,
    const std::initializer_list<uint8_t>& patchBytes)
    : Toggleable(name), _target(target), _patched(patchBytes), _flushInstructionCache(flushInstructionCache) {}

memory::BytePatch::~BytePatch()
{
    if (enabled())
        disable();
}

std::shared_ptr<memory::BytePatch> memory::BytePatch::create(
    const std::string&                    name,
    const Handle&                         target,
    bool                                  flushInstructionCache,
    const std::initializer_list<uint8_t>& patchBytes)
{
    return std::make_shared < BytePatch > (name, target, flushInstructionCache, patchBytes);
}

memory::NopPatch::NopPatch(const std::string& name, const Handle& target, const size_t size)
    : BytePatch(name, target, true, {})
{
    _patched = std::vector<uint8_t>(size, 0x90);
}

std::shared_ptr<memory::NopPatch> memory::NopPatch::create(const std::string& name, const Handle& target, size_t size)
{
    return std::make_shared < NopPatch > (name, target, size);
}

bool memory::RefNopPatch::internalEnable()
{
    auto level = logging::LogLevel::Info;
    if (logging::Logger::instance())
    {
        level = logging::Logger::instance()->level();
        logging::Logger::instance()->setLevel(logging::LogLevel::Warning);
    }

    for (const auto& patch : _patches)
    {
        if (!patch->enable())
        {
            LOG_ERR("Failed to enable sub-patch \"{}\"", patch->name());
            goto disable;
        }
    }

    if (logging::Logger::instance())
    {
        logging::Logger::instance()->setLevel(level);
    }

    return true;
disable:
    for (const auto& patch : _patches)
    {
        if (patch->enabled())
            patch->disable();
    }

    if (logging::Logger::instance())
    {
        logging::Logger::instance()->setLevel(level);
    }

    return false;
}

bool memory::RefNopPatch::internalDisable()
{
    auto level = logging::LogLevel::Info;

    if (logging::Logger::instance())
    {
        level = logging::Logger::instance()->level();
        logging::Logger::instance()->setLevel(logging::LogLevel::Warning);
    }

    for (const auto& patch : _patches)
    {
        if (!patch->disable())
        {
            LOG_ERR("Failed to disable sub-patch \"{}\"", patch->name());
            goto enable;
        }
    }

    if (logging::Logger::instance())
    {
        logging::Logger::instance()->setLevel(level);
    }

    return true;
enable:
    for (const auto& patch : _patches)
    {
        if (!patch->enabled())
            patch->enable();
    }

    if (logging::Logger::instance())
    {
        logging::Logger::instance()->setLevel(level);
    }

    return false;
}

memory::RefNopPatch::RefNopPatch(
    const std::string&  name,
    Module&             module,
    const Handle&       target,
    const RefData::Type refType)
    : Toggleable(name)
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
            NopPatch::create(fmt::format("{}[{}]", this->name(), count), ref.instruction(), ref.instructionLength()));
        ++count;
    }
}

memory::RefNopPatch::~RefNopPatch()
{
    if (enabled())
        disable();
}

std::shared_ptr<memory::RefNopPatch> memory::RefNopPatch::create(
    const std::string& name,
    Module&            module,
    const Handle&      target,
    RefData::Type      refType)
{
    return std::make_shared < RefNopPatch > (name, module, target, refType);
}

bool memory::StringRefPatch::internalEnable()
{
    if (_allocation.raw() == 0 || _allocationSize == 0)
    {
        LOG_ERR("Invalid allocation (did you forget to set the string?)");
        return false;
    }

    const Protection protection(_lea, 7, PAGE_EXECUTE_READWRITE);
    if (!protection.success())
    {
        LOG_ERR("Failed to protect memory");
        return false;
    }

    *_lea.add(3).to_ptr<int32_t*>() = static_cast<int32_t>(_allocation.raw() - _lea.add(7).raw());

    return true;
}

bool memory::StringRefPatch::internalDisable()
{
    const Protection protection(_lea, 7, PAGE_EXECUTE_READWRITE);
    if (!protection.success())
    {
        LOG_ERR("Failed to protect memory");
        return false;
    }

    *_lea.add(3).to_ptr<int32_t*>() = static_cast<int32_t>(_originalString.raw() - _lea.add(7).raw());

    return true;
}

memory::StringRefPatch::StringRefPatch(std::string name, const RefData& ref)
    : Toggleable(std::move(name))
{
    _lea            = ref.instruction();
    _originalString = ref.reference();
}

memory::StringRefPatch::~StringRefPatch()
{
    if (enabled())
        disable();
}

void memory::StringRefPatch::setString(const std::string& string)
{
    const bool wasEnabled = enabled();

    LOG_INFO("Setting \"{}\" text to \"{}\"", name(), string);

    if (wasEnabled && !disable())
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

    if (wasEnabled && !enable())
    {
        LOG_ERR("Failed to re-enable patch");
    }

    LOG_INFO("Text set");
}

void memory::StringRefPatch::setWstring(const std::wstring& string)
{
    const bool wasEnabled = enabled();

    LOG_INFO("Setting \"{}\" text to \"{}\"", name(), util::wstringToString(string));

    if (wasEnabled && !disable())
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

    if (wasEnabled && !enable())
    {
        LOG_ERR("Failed to re-enable patch");
    }

    LOG_INFO("Text set");
}

std::shared_ptr<memory::StringRefPatch> memory::StringRefPatch::create(const std::string& name, const RefData& lea)
{
    return std::make_shared < StringRefPatch > (name, lea);
}
