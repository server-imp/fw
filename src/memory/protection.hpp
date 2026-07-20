#ifndef FW_PROTECTION_HPP
#define FW_PROTECTION_HPP
#include "memory.hpp"

namespace memory
{
    struct ProtectedRegion
    {
        Range range {};
        DWORD oldProtect {};

        [[nodiscard]] Handle    start() const;
        [[nodiscard]] Handle    end() const;
        [[nodiscard]] ptrdiff_t size() const;
    };

    class Protection
    {
    private:
        std::vector<ProtectedRegion> _regions;

    public:
        Protection(const Handle& base, size_t size, DWORD protection);
        Protection(const Range& range, DWORD protection);

        ~Protection();
        Protection(const Protection&)            = delete;
        Protection& operator=(const Protection&) = delete;

        [[nodiscard]] const std::vector<ProtectedRegion>& regions() const;
    };
}

#endif //FW_PROTECTION_HPP
