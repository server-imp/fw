#ifndef FW_WNDPROC_HPP
#define FW_WNDPROC_HPP
#pragma once
#include "memory/hook.hpp"

namespace memory::hooks
{
    class WndProc final : Hook
    {
    private:
        static WndProc* _instance;
        HWND            _hWnd {};
        WNDPROC         _originalWndProc {};

        std::vector<std::function<uintptr_t(HWND, UINT, WPARAM, LPARAM)>> _callbacks {};

    public:
        explicit WndProc(HWND hWnd);

        ~WndProc();

        virtual bool enable();

        virtual bool disable(bool uninitialize);

        void addCallback(const std::function<uintptr_t(HWND, UINT, WPARAM, LPARAM)>& callback);

    private:
        LRESULT CALLBACK internalWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) const;

        static LRESULT CALLBACK wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    public:
    };
}

#endif //FW_WNDPROC_HPP
