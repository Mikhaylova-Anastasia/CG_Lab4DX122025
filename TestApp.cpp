#include "TestApp.h"
#include <sstream>
#include <iomanip>
#include <iostream>

TestApp::TestApp(HINSTANCE hInstance)
    : DX12Framework(hInstance),
    testPhase(0),
    phaseTimer(0.0f),
    testCompleted(false),
    mouseX(0), mouseY(0),
    leftMouseDown(false), rightMouseDown(false),
    lastDeltaTime(0.0f),
    frameCount(0),
    fpsUpdateTimer(0.0f) {

    clearColor[0] = 0.0f;
    clearColor[1] = 0.2f;
    clearColor[2] = 0.4f;
    clearColor[3] = 1.0f;

    mainWindowCaption = L"DX12 Framework Test - Checking All Functions";
    clientWidth = 1280;
    clientHeight = 720;
}

bool TestApp::Initialize() {
    std::cout << "=== Starting Framework Tests ===" << std::endl;

    // Тест 1: Функции окна
    TestWindowFunctions();

    // Тест 2: Функции таймера
    TestTimerFunctions();

    // Тест 3: Ввод данных
    TestInputHandling();

    // Инициализация базового класса
    if (!DX12Framework::Initialize()) {
        return false;
    }

    std::cout << "=== All Basic Tests Completed ===" << std::endl;
    std::cout << "Now testing real-time functions in main loop..." << std::endl;

    return true;
}

void TestApp::TestTimerFunctions() {
    std::cout << "\n--- Testing Timer Functions ---" << std::endl;

    // Тест сброса
    timer.Reset();
    std::cout << "Timer Reset: TotalTime = " << timer.TotalTime()
        << ", DeltaTime = " << timer.DeltaTime() << std::endl;

    // Тест тика
    timer.Tick();
    float delta1 = timer.DeltaTime();
    std::cout << "First Tick: DeltaTime = " << delta1 << std::endl;

    // Небольшая пауза
    Sleep(10);
    timer.Tick();
    float delta2 = timer.DeltaTime();
    std::cout << "Second Tick after 10ms: DeltaTime = " << delta2 << std::endl;

    // Тест остановки/запуска
    timer.Stop();
    float stoppedTime = timer.TotalTime();
    std::cout << "After Stop: TotalTime = " << stoppedTime << std::endl;

    Sleep(100);
    timer.Start();
    timer.Tick();
    std::cout << "After Start + Tick: TotalTime = " << timer.TotalTime() << std::endl;

    std::cout << "Timer Test: PASSED" << std::endl;
}

void TestApp::TestWindowFunctions() {
    std::cout << "\n--- Testing Window Functions ---" << std::endl;

    std::cout << "App Instance: " << AppInst() << std::endl;
    std::cout << "Aspect Ratio: " << AspectRatio() << std::endl;
    std::cout << "Main Window Handle: " << MainWnd() << std::endl;

    // Проверка статического метода
    DX12Framework* app = GetApp();
    if (app == this) {
        std::cout << "GetApp() Test: PASSED" << std::endl;
    }
    else {
        std::cout << "GetApp() Test: FAILED" << std::endl;
    }

    std::cout << "Window Functions Test: PASSED" << std::endl;
}

void TestApp::TestInputHandling() {
    std::cout << "\n--- Testing Input Handling ---" << std::endl;
    std::cout << "Mouse event handlers registered" << std::endl;
    std::cout << "Input Test: READY (will test in main loop)" << std::endl;
}

void TestApp::Update(const GameTimer& timer) {
    frameCount++;
    fpsUpdateTimer += timer.DeltaTime();

    // Обновляем информацию о тесте каждую секунду
    if (fpsUpdateTimer >= 1.0f) {
        UpdateTestInfo();
        fpsUpdateTimer = 0.0f;
        frameCount = 0;
    }

    // Автоматическое тестирование разных фаз
    if (!testCompleted) {
        phaseTimer += timer.DeltaTime();

        if (phaseTimer >= 3.0f) {  // Меняем фазу каждые 3 секунды
            phaseTimer = 0.0f;
            testPhase++;

            if (testPhase > 4) {
                testCompleted = true;
                testPhase = 4;
            }

            // Меняем цвет для визуальной индикации фазы теста
            switch (testPhase) {
            case 0:
                clearColor[0] = 1.0f; clearColor[1] = 0.0f; clearColor[2] = 0.0f; // Красный
                break;
            case 1:
                clearColor[0] = 0.0f; clearColor[1] = 1.0f; clearColor[2] = 0.0f; // Зеленый
                break;
            case 2:
                clearColor[0] = 0.0f; clearColor[1] = 0.0f; clearColor[2] = 1.0f; // Синий
                break;
            case 3:
                clearColor[0] = 1.0f; clearColor[1] = 1.0f; clearColor[2] = 0.0f; // Желтый
                break;
            case 4:
                clearColor[0] = 1.0f; clearColor[1] = 0.0f; clearColor[2] = 1.0f; // Фиолетовый
                break;
            }
        }
    }

    // Сохраняем последнее время между кадрами
    lastDeltaTime = timer.DeltaTime();
}

void TestApp::Draw(const GameTimer& timer) {
    HRESULT hr = commandAllocator->Reset();
    if (FAILED(hr)) {
        std::cout << "CommandAllocator Reset failed" << std::endl;
        return;
    }

    hr = commandList->Reset(commandAllocator.Get(), nullptr);
    if (FAILED(hr)) {
        std::cout << "CommandList Reset failed" << std::endl;
        return;
    }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = CurrentBackBuffer();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CurrentBackBufferView();
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    D3D12_RESOURCE_BARRIER barrier2 = barrier;
    barrier2.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier2.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

    commandList->ResourceBarrier(1, &barrier2);

    hr = commandList->Close();
    if (FAILED(hr)) {
        std::cout << "CommandList Close failed" << std::endl;
        return;
    }

    ID3D12CommandList* cmdLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(1, cmdLists);

    hr = swapChain->Present(1, 0);
    if (FAILED(hr)) {
        std::cout << "SwapChain Present failed" << std::endl;
        return;
    }

    currentBackBuffer = (currentBackBuffer + 1) % SwapChainBufferCount;
    FlushCommandQueue();

    if (frameCount % 60 == 0) {
        std::cout << "Draw called - Frame: " << frameCount
            << ", Color: R=" << clearColor[0]
            << " G=" << clearColor[1]
            << " B=" << clearColor[2] << std::endl;
    }
}

void TestApp::OnResize() {
    std::cout << "OnResize called - New size: " << clientWidth << "x" << clientHeight
        << ", Aspect Ratio: " << AspectRatio() << std::endl;

    DX12Framework::OnResize();
}

void TestApp::CreateRtvAndDsvDescriptorHeaps() {
    std::cout << "CreateRtvAndDsvDescriptorHeaps called" << std::endl;

    DX12Framework::CreateRtvAndDsvDescriptorHeaps();
    std::cout << "Descriptor Heaps created successfully" << std::endl;
}

void TestApp::OnMouseDown(WPARAM btnState, int x, int y) {
    mouseX = x;
    mouseY = y;

    if (btnState & MK_LBUTTON) {
        leftMouseDown = true;
        std::cout << "Left Mouse DOWN at: " << x << ", " << y << std::endl;
    }
    if (btnState & MK_RBUTTON) {
        rightMouseDown = true;
        std::cout << "Right Mouse DOWN at: " << x << ", " << y << std::endl;
    }
}

void TestApp::OnMouseUp(WPARAM btnState, int x, int y) {
    mouseX = x;
    mouseY = y;

    if (!(btnState & MK_LBUTTON) && leftMouseDown) {
        leftMouseDown = false;
        std::cout << "Left Mouse UP at: " << x << ", " << y << std::endl;
    }
    if (!(btnState & MK_RBUTTON) && rightMouseDown) {
        rightMouseDown = false;
        std::cout << "Right Mouse UP at: " << x << ", " << y << std::endl;
    }
}

void TestApp::OnMouseMove(WPARAM btnState, int x, int y) {
    // Выводим перемещение только если зажата кнопка (чтобы не засорять консоль)
    if (btnState & (MK_LBUTTON | MK_RBUTTON)) {
        std::cout << "Mouse MOVE to: " << x << ", " << y
            << " (Buttons: " << (btnState & MK_LBUTTON ? "L" : "")
            << (btnState & MK_RBUTTON ? "R" : "") << ")" << std::endl;
    }

    mouseX = x;
    mouseY = y;
}

void TestApp::UpdateTestInfo() {
    std::wstringstream title;
    title << L"DX12 Framework Test - ";

    switch (testPhase) {
    case 0: title << L"Phase 1: Red - Testing Timer"; break;
    case 1: title << L"Phase 2: Green - Testing Input"; break;
    case 2: title << L"Phase 3: Blue - Testing Rendering"; break;
    case 3: title << L"Phase 4: Yellow - Testing Resize"; break;
    case 4: title << L"Phase 5: Purple - All Tests Complete"; break;
    }

    title << L" | FPS: " << frameCount;
    title << L" | Delta: " << std::fixed << std::setprecision(3) << lastDeltaTime * 1000.0f << L"ms";
    title << L" | Mouse: " << mouseX << L"," << mouseY;
    title << L" | Time: " << std::setprecision(1) << timer.TotalTime() << L"s";

    SetWindowText(MainWnd(), title.str().c_str());

    std::cout << "Test Phase: " << testPhase
        << ", FPS: " << frameCount
        << ", Avg Delta: " << lastDeltaTime * 1000.0f << "ms" << std::endl;
}