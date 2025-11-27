#include "Window.h"
#include "InputDevice.h"
#include <Windows.h>

static Window* g_Window = nullptr;

Window::Window(const std::wstring& title, int width, int height)
    : title(title), width(width), height(height), hwnd(nullptr), inputDevice(nullptr) {
}

bool Window::Create() {
    g_Window = this;

    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = Window::WndProcRouter;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"DX12WindowClass";

    RegisterClassEx(&wc);

    hwnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        nullptr, nullptr,
        wc.hInstance,
        nullptr
    );

    ShowWindow(hwnd, SW_SHOW);
    return hwnd != nullptr;
}

bool Window::ProcessMessages() {
    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT)
            return false;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}

LRESULT CALLBACK Window::WndProcRouter(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (g_Window)
        return g_Window->WndProc(hWnd, msg, wParam, lParam);

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_CHAR:
        if (inputDevice) {
            wchar_t ch = static_cast<wchar_t>(wParam);
            if (ch == 8) inputDevice->Backspace();
            else if (ch >= 32) inputDevice->AddChar(ch);
        }
        break;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}