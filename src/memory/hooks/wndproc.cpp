#include "wndproc.hpp"

memory::hooks::WndProc* memory::hooks::WndProc::_instance {};

bool memory::hooks::WndProc::internalEnable()
{
    _originalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(
        _hWnd,
        GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(wndProc)));

    if (!_originalWndProc)
    {
        return false;
    }

    return true;
}

bool memory::hooks::WndProc::internalDisable()
{
    SetWindowLongPtr(_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(_originalWndProc));

    return true;
}

memory::hooks::WndProc::WndProc(HWND hWnd)
    : Hook("WndProc Hook", nullptr, nullptr, nullptr)
{
    _instance = this;
    _hWnd     = hWnd;
}

memory::hooks::WndProc::~WndProc()
{
    if (enabled())
        disable();
    _instance = nullptr;
}

void memory::hooks::WndProc::addCallback(const std::function<uintptr_t(HWND, UINT, WPARAM, LPARAM)>& callback)
{
    _callbacks.push_back(callback);
}

LRESULT memory::hooks::WndProc::internalWndProc(
    HWND         hWnd,
    const UINT   msg,
    const WPARAM wParam,
    const LPARAM lParam) const
{
    auto callOriginal = true;

    for (const auto& callback : _callbacks)
    {
        if (callback(hWnd, msg, wParam, lParam) == 0)
        {
            callOriginal = false;
            break;
        }
    }

    if (!callOriginal)
    {
        return 0;
    }

    return CallWindowProc(_originalWndProc, hWnd, msg, wParam, lParam);
}

LRESULT memory::hooks::WndProc::wndProc(HWND hWnd, const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
    if (!_instance)
    {
        return 0;
    }

    return _instance->internalWndProc(hWnd, msg, wParam, lParam);
}
