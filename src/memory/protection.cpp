#include "protection.hpp"

#include "logger.hpp"
#include "util.hpp"

memory::Handle memory::ProtectedRegion::start() const
{
    return range.start();
}

memory::Handle memory::ProtectedRegion::end() const
{
    return range.end();
}

ptrdiff_t memory::ProtectedRegion::size() const
{
    return range.size();
}

memory::Protection::Protection(const Handle& base, const size_t size, const DWORD protection)
{
    LOG_DBG("Protecting {:08X}+{:04X} [{:08X}]", base.raw(), size, protection);

    if (!base.raw() || !size)
    {
        LOG_DBG("Invalid protection range");
        return;
    }

    const Handle end = base.add(static_cast<std::ptrdiff_t>(size));

    for (Handle current = base; current < end;)
    {
        MEMORY_BASIC_INFORMATION info {};
        if (!VirtualQuery(current.to_ptr<void*>(), &info, sizeof(info)))
        {
            LOG_DBG("VirtualQuery failed: {:08X} [{}]", current.raw(), GetLastError());
            rollback();
            return;
        }

        if (info.State != MEM_COMMIT)
        {
            LOG_DBG("Memory not committed: {:08X}", current.raw());
            rollback();
            return;
        }

        const Handle regionBegin(info.BaseAddress);
        const Handle regionEnd = regionBegin.add(static_cast<std::ptrdiff_t>(info.RegionSize));

        const Handle protectBegin = current;
        const Handle protectEnd   = std::min(end, regionEnd);

        const auto protectSize = protectEnd.raw() - protectBegin.raw();

        DWORD oldProtect {};
        if (!VirtualProtect(protectBegin.to_ptr<void*>(), protectSize, protection, &oldProtect))
        {
            LOG_DBG("VirtualProtect failed: {:08X} [{}]", protectBegin.raw(), GetLastError());

            rollback();
            return;
        }

        _regions.emplace_back(ProtectedRegion { .range = Range(protectBegin, protectEnd), .oldProtect = oldProtect });

        current = protectEnd;
    }
    _success = true;
    LOG_DBG("Protection completed");
}

memory::Protection::~Protection()
{
    rollback();
}

bool memory::Protection::success() const
{
    return _success;
}

const std::vector<memory::ProtectedRegion>& memory::Protection::regions() const
{
    return _regions;
}

void memory::Protection::rollback() noexcept
{
    LOG_DBG("Rolling back protection");

    DWORD ignored {};
    for (auto it = _regions.rbegin(); it != _regions.rend(); ++it)
    {
        VirtualProtect(it->range.start().to_ptr<void*>(), it->range.size(), it->oldProtect, &ignored);
    }

    _regions.clear();
}
