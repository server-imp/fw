#ifndef FW_HANDLE_HPP
#define FW_HANDLE_HPP
#pragma once

namespace memory
{
    class Handle
    {
    private:
        uintptr_t _pointer {};

    public:
        Handle() = default;

        explicit Handle(void* pointer);

        explicit Handle(uint64_t pointer);

        Handle(const Handle& other);

        [[nodiscard]] uintptr_t raw() const;

        [[nodiscard]] Handle add(ptrdiff_t offset) const;

        [[nodiscard]] Handle add(const Handle& other) const;

        [[nodiscard]] Handle sub(ptrdiff_t offset) const;

        [[nodiscard]] Handle sub(const Handle& other) const;

        [[nodiscard]] Handle rip() const;

        [[nodiscard]] Handle resolve_relative_call() const;

        template <typename T>
        std::enable_if_t<std::is_pointer_v<T>, T> to_ptr() const
        {
            return reinterpret_cast<T>(_pointer);
        }

        template <typename T>
        std::enable_if_t<std::is_object_v<T>, T&> deref() const
        {
            return *to_ptr<T*>();
        }

        bool operator==(const Handle& other) const noexcept;

        bool operator==(uintptr_t other) const noexcept;

        bool operator!=(uintptr_t other) const noexcept;

        bool operator!=(const Handle& other) const noexcept;

        bool operator<(const Handle& other) const noexcept;

        bool operator<(uintptr_t other) const noexcept;

        bool operator<=(const Handle& other) const noexcept;

        bool operator<=(uintptr_t other) const noexcept;

        bool operator>(const Handle& other) const noexcept;

        bool operator>(uintptr_t other) const noexcept;

        bool operator>=(const Handle& other) const noexcept;

        bool operator>=(uintptr_t other) const noexcept;
    };
}

#endif //FW_HANDLE_HPP
