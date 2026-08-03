#ifndef FW_WNDPROC_HPP
#define FW_WNDPROC_HPP
#pragma once
#include "memory/hook.hpp"

namespace memory
{
    namespace hooks
    {
        class WndProc final : public Hook
        {
        private:
            static WndProc* _instance;
            HWND            _hWnd {};
            WNDPROC         _originalWndProc {};

            std::vector<std::function<uintptr_t(HWND, UINT, WPARAM, LPARAM)>> _callbacks {};

        protected:
            bool internalEnable() override;
            bool internalDisable() override;

        public:
            explicit WndProc(HWND hWnd);

            ~WndProc() override;

            void addCallback(const std::function<uintptr_t(HWND, UINT, WPARAM, LPARAM)>& callback);

        private:
            LRESULT CALLBACK internalWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) const;

            static LRESULT CALLBACK wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
        };
    } // namespace hooks
} // namespace memory

#endif // FW_WNDPROC_HPP
