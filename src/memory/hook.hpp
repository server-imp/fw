#ifndef FW_HOOK_HPP
#define FW_HOOK_HPP
#pragma once

#include "pch.hpp"
#include "toggleable.hpp"

namespace memory
{
    class Hook : public Toggleable
    {
    protected:
        void* _target {};
        void* _original {};
        void* _ownFunction {};

        Hook(std::string name, void* target, void* original, void* ownFunction);

    public:
        [[nodiscard]] void* target() const;

        template<typename T>
        T original() const;
    };

    using PHook = std::shared_ptr<Hook>;

    template<typename T>
    T Hook::original() const
    {
        return reinterpret_cast<T>(_original);
    }

    class Detour final : public Hook
    {
    public:
        Detour(std::string name, void* target, void* ownFunction);

        ~Detour() override;

    protected:
        bool internalEnable() override;
        bool internalDisable() override;
    };

    struct HookScope
    {
        std::atomic_uint32_t& counter;

        explicit HookScope(std::atomic_uint32_t& counter);
        ~HookScope();
    };
} // namespace memory

#endif // FW_HOOK_HPP
