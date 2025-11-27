#include "Application.h"
#include <windows.h>

Application::Application() {
    input = std::make_unique<InputDevice>();
    window = std::make_unique<Window>(L"DX12 Window - Type text and press 1,2,3,0", 1280, 720);
}

bool Application::Initialize() {
    if (!window->Create())
        return false;

    window->inputDevice = input.get();
    render = std::make_unique<DX12Render>(window->GetHWND(), 1280, 720);
    return true;
}

int Application::Run() {
    if (!Initialize())
        return -1;

    while (window->ProcessMessages()) {
        Tick();

        std::wstring userInput = input->GetText();
        if (!userInput.empty()) {

            std::wstring newTitle = L"Text: " + userInput + L" | Press 1-Red, 2-Green, 3-Blue, 0-Reset | ESC to exit";
            SetWindowText(window->GetHWND(), newTitle.c_str());
        }

        if (GetAsyncKeyState('1') & 0x8000) {
            render->HandleKeyInput('1');

            Sleep(100);
        }
        if (GetAsyncKeyState('2') & 0x8000) {
            render->HandleKeyInput('2');
            Sleep(100);
        }
        if (GetAsyncKeyState('3') & 0x8000) {
            render->HandleKeyInput('3');
            Sleep(100);
        }
        if (GetAsyncKeyState('0') & 0x8000) {
            render->HandleKeyInput('0');
            Sleep(100);
        }

        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
            break;
    }

    return 0;
}

void Application::Tick() {
    render->Render();
}