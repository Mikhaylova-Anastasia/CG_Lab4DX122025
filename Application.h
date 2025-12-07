#pragma once
#include <memory>
#include "Window.h"
#include "InputDevice.h"
#include "DX12Render.h"
#include "Timer.h"  // Добавляем таймер

class Application {
public:
    Application();
    int Run();

private:
    bool Initialize();
    void Tick();

private:
    std::unique_ptr<Window> window;
    std::unique_ptr<InputDevice> input;
    std::unique_ptr<DX12Render> render;
    Timer timer;  // Добавляем таймер
};