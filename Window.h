#pragma once
#include <string>
#include <Windows.h>

class InputDevice;

class Window {
public:
    Window(const std::wstring& title, int width, int height);

    bool Create();
    bool ProcessMessages();
    HWND GetHWND() const { return hwnd; }

    InputDevice* inputDevice = nullptr;

private:
    static LRESULT CALLBACK WndProcRouter(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    std::wstring title;
    int width, height;
    HWND hwnd;
};