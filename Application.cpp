#include "Application.h"
#include <windows.h>
#include <sstream>
#include <iomanip>

Application::Application() {
    input = std::make_unique<InputDevice>();
    window = std::make_unique<Window>(L"DX12 Pyramid - Press SPACE for wireframe", 1280, 720);
    timer.Reset();
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
        timer.Tick();

        render->Update(timer.DeltaTime());
        render->Render();

        std::wstring userInput = input->GetText();

        std::wstringstream titleStream;
        titleStream << L"DX12 Pyramid | FPS: " << std::setprecision(0) << (timer.DeltaTime() > 0 ? 1.0f / timer.DeltaTime() : 0.0f);
        titleStream << L" | Time: " << std::fixed << std::setprecision(1) << timer.TotalTime() << L"s";
        titleStream << L" | Press 1-Red, 2-Green, 3-Blue, 0-Dark blue, SPACE-Wireframe";

        SetWindowText(window->GetHWND(), titleStream.str().c_str());

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