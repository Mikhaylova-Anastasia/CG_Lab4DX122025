#include "Framework.h"
#include <iostream>
#include <stdexcept>
#include <windowsx.h>

DX12Framework* DX12Framework::appInstance = nullptr;

GameTimer::GameTimer()
    : secondsPerCount(0.0), deltaTime(-1.0), baseTime(0),
    pausedTime(0), prevTime(0), currTime(0), stopped(false) {
    __int64 countsPerSec;
    QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
    secondsPerCount = 1.0 / (double)countsPerSec;
}

float GameTimer::TotalTime() const {
    if (stopped) {
        return (float)(((stopTime - pausedTime) - baseTime) * secondsPerCount);
    }
    else {
        return (float)(((currTime - pausedTime) - baseTime) * secondsPerCount);
    }
}

float GameTimer::DeltaTime() const {
    return (float)deltaTime;
}

void GameTimer::Reset() {
    __int64 currTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
    baseTime = currTime;
    prevTime = currTime;
    stopTime = 0;
    stopped = false;
}

void GameTimer::Start() {
    __int64 startTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&startTime);
    if (stopped) {
        pausedTime += (startTime - stopTime);
        prevTime = startTime;
        stopTime = 0;
        stopped = false;
    }
}

void GameTimer::Stop() {
    if (!stopped) {
        __int64 currTime;
        QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
        stopTime = currTime;
        stopped = true;
    }
}

void GameTimer::Tick() {
    if (stopped) {
        deltaTime = 0.0;
        return;
    }
    __int64 currTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
    this->currTime = currTime;
    deltaTime = (this->currTime - prevTime) * secondsPerCount;
    prevTime = this->currTime;
    if (deltaTime < 0.0) {
        deltaTime = 0.0;
    }
}

DX12Framework::DX12Framework(HINSTANCE hInstance)
    : appInstanceHandle(hInstance),
    clientWidth(1280),
    clientHeight(720),
    appPaused(false),
    minimized(false),
    maximized(false),
    resizing(false),
    currentFence(0),
    currentBackBuffer(0) {
    appInstance = this;
}

DX12Framework::~DX12Framework() {
    if (device != nullptr)
        FlushCommandQueue();
}

DX12Framework* DX12Framework::GetApp() {
    return appInstance;
}

HINSTANCE DX12Framework::AppInst() const {
    return appInstanceHandle;
}

HWND DX12Framework::MainWnd() const {
    return mainWindowHandle;
}

float DX12Framework::AspectRatio() const {
    return static_cast<float>(clientWidth) / clientHeight;
}

void DX12Framework::UpdateWindowTitle(const GameTimer& timer) {
    std::wstringstream titleStream;
    titleStream << mainWindowCaption;
    titleStream << L" | Time: " << std::fixed << std::setprecision(1) << timer.TotalTime() << L"s";
    titleStream << L" | FPS: " << std::setprecision(0) << (timer.DeltaTime() > 0 ? 1.0f / timer.DeltaTime() : 0.0f);
    SetWindowText(mainWindowHandle, titleStream.str().c_str());
}

int DX12Framework::Run() {
    MSG msg = { 0 };
    timer.Reset();
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            timer.Tick();
            if (!appPaused) {
                Update(timer);
                Draw(timer);
                UpdateWindowTitle(timer);
            }
            else {
                Sleep(100);
            }
        }
    }
    return (int)msg.wParam;
}

LRESULT DX12Framework::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        clientWidth = LOWORD(lParam);
        clientHeight = HIWORD(lParam);
        if (device) {
            if (wParam == SIZE_MINIMIZED) {
                appPaused = true;
                minimized = true;
                maximized = false;
            }
            else if (wParam == SIZE_MAXIMIZED) {
                appPaused = false;
                minimized = false;
                maximized = true;
                OnResize();
            }
            else if (wParam == SIZE_RESTORED) {
                if (minimized) {
                    appPaused = false;
                    minimized = false;
                    OnResize();
                }
                else if (maximized) {
                    appPaused = false;
                    maximized = false;
                    OnResize();
                }
                else if (!resizing) {
                    OnResize();
                }
            }
        }
        return 0;

    case WM_ENTERSIZEMOVE:
        appPaused = true;
        resizing = true;
        timer.Stop();
        return 0;

    case WM_EXITSIZEMOVE:
        appPaused = false;
        resizing = false;
        timer.Start();
        OnResize();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_MENUCHAR:
        return MAKELRESULT(0, MNC_CLOSE);

    case WM_GETMINMAXINFO:
        ((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
        ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
        return 0;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_MOUSEMOVE:
        OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_KEYUP:
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
        }
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool DX12Framework::Initialize() {
    if (!InitMainWindow())
        return false;
    if (!InitDirect3D())
        return false;
    OnResize();
    return true;
}

void DX12Framework::CreateRtvAndDsvDescriptorHeaps() {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)));
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap)));
}

void DX12Framework::OnResize() {
    if (!device) return;
    if (!swapChain) return;

    FlushCommandQueue();

    commandList->Reset(commandAllocator.Get(), nullptr);

    for (int i = 0; i < SwapChainBufferCount; ++i) {
        swapChainBuffer[i].Reset();
    }

    HRESULT hr = swapChain->ResizeBuffers(
        SwapChainBufferCount,
        clientWidth,
        clientHeight,
        backBufferFormat,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
    );

    if (FAILED(hr)) {
        MessageBox(0, L"ResizeBuffers failed", L"Error", MB_OK);
        return;
    }

    currentBackBuffer = swapChain->GetCurrentBackBufferIndex();

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < SwapChainBufferCount; i++) {
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainBuffer[i]));
        if (FAILED(hr)) {
            MessageBox(0, L"GetBuffer failed after resize", L"Error", MB_OK);
            return;
        }
        device->CreateRenderTargetView(swapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
        rtvHeapHandle.ptr += rtvDescriptorSize;
    }

    commandList->Close();
    ID3D12CommandList* cmdLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(1, cmdLists);

    FlushCommandQueue();

    screenViewport.Width = static_cast<float>(clientWidth);
    screenViewport.Height = static_cast<float>(clientHeight);
    scissorRect = { 0, 0, clientWidth, clientHeight };

    std::cout << "DX12Framework::OnResize() called - Size: "
        << clientWidth << "x" << clientHeight << std::endl;
}

bool DX12Framework::InitMainWindow() {
    WNDCLASS wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
        return DX12Framework::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
        };
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = appInstanceHandle;
    wc.hIcon = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = L"DX12FrameworkWndClass";
    if (!RegisterClass(&wc)) {
        MessageBox(0, L"RegisterClass Failed.", 0, 0);
        return false;
    }
    RECT R = { 0, 0, clientWidth, clientHeight };
    AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
    int width = R.right - R.left;
    int height = R.bottom - R.top;
    mainWindowHandle = CreateWindow(
        L"DX12FrameworkWndClass",
        mainWindowCaption.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        0, 0,
        appInstanceHandle,
        0
    );
    if (!mainWindowHandle) {
        MessageBox(0, L"CreateWindow Failed.", 0, 0);
        return false;
    }
    ShowWindow(mainWindowHandle, SW_SHOW);
    UpdateWindow(mainWindowHandle);
    return true;
}

bool DX12Framework::InitDirect3D() {
    UINT dxgiFactoryFlags = 0;

    HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        MessageBox(0, L"CreateDXGIFactory2 failed", L"Error", MB_OK);
        return false;
    }

    hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(hr)) {
        MessageBox(0, L"D3D12CreateDevice failed", L"Error", MB_OK);
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
    if (FAILED(hr)) {
        MessageBox(0, L"CreateCommandQueue failed", L"Error", MB_OK);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = SwapChainBufferCount;
    swapChainDesc.Width = clientWidth;
    swapChainDesc.Height = clientHeight;
    swapChainDesc.Format = backBufferFormat;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain1;
    hr = factory->CreateSwapChainForHwnd(
        commandQueue.Get(),
        mainWindowHandle,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    );

    if (FAILED(hr)) {
        MessageBox(0, L"CreateSwapChainForHwnd failed", L"Error", MB_OK);
        return false;
    }

    hr = swapChain1.As(&swapChain);  // Конвертируем в IDXGISwapChain3
    if (FAILED(hr)) {
        MessageBox(0, L"QueryInterface for swap chain 3 failed", L"Error", MB_OK);
        return false;
    }

    currentBackBuffer = swapChain->GetCurrentBackBufferIndex();  // Теперь работает

    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CreateRtvAndDsvDescriptorHeaps();

    for (UINT i = 0; i < SwapChainBufferCount; i++) {
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainBuffer[i]));
        if (FAILED(hr)) {
            MessageBox(0, L"GetBuffer failed", L"Error", MB_OK);
            return false;
        }
    }

    hr = device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&commandAllocator)
    );
    if (FAILED(hr)) {
        MessageBox(0, L"CreateCommandAllocator failed", L"Error", MB_OK);
        return false;
    }

    hr = device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&commandList)
    );
    if (FAILED(hr)) {
        MessageBox(0, L"CreateCommandList failed", L"Error", MB_OK);
        return false;
    }

    commandList->Close();

    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (FAILED(hr)) {
        MessageBox(0, L"CreateFence failed", L"Error", MB_OK);
        return false;
    }

    screenViewport.TopLeftX = 0;
    screenViewport.TopLeftY = 0;
    screenViewport.Width = static_cast<float>(clientWidth);
    screenViewport.Height = static_cast<float>(clientHeight);
    screenViewport.MinDepth = 0.0f;
    screenViewport.MaxDepth = 1.0f;

    scissorRect = { 0, 0, clientWidth, clientHeight };

    return true;
}

void DX12Framework::FlushCommandQueue() {
    std::cout << "DX12Framework::FlushCommandQueue() called" << std::endl;
}

ID3D12Resource* DX12Framework::CurrentBackBuffer() const {
    return swapChainBuffer[currentBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12Framework::CurrentBackBufferView() const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += currentBackBuffer * rtvDescriptorSize;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12Framework::DepthStencilView() const {
    return dsvHeap->GetCPUDescriptorHandleForHeapStart();
}