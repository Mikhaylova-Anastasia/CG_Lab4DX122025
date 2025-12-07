#pragma once
#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <string>
#include <sstream>
#include <iomanip>

using Microsoft::WRL::ComPtr;

class GameTimer {
public:
    GameTimer();

    float TotalTime() const;
    float DeltaTime() const;

    void Reset();
    void Start();
    void Stop();
    void Tick();

private:
    double secondsPerCount;
    double deltaTime;

    __int64 baseTime;
    __int64 pausedTime;
    __int64 stopTime;
    __int64 prevTime;
    __int64 currTime;

    bool stopped;
};

class DX12Framework {
public:
    DX12Framework(HINSTANCE hInstance);
    virtual ~DX12Framework();

    static DX12Framework* GetApp();

    HINSTANCE AppInst() const;
    HWND MainWnd() const;
    float AspectRatio() const;

    int Run();

    virtual bool Initialize();
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    virtual void CreateRtvAndDsvDescriptorHeaps();
    virtual void OnResize();
    virtual void Update(const GameTimer& timer) = 0;
    virtual void Draw(const GameTimer& timer) = 0;

    virtual void OnMouseDown(WPARAM btnState, int x, int y) {}
    virtual void OnMouseUp(WPARAM btnState, int x, int y) {}
    virtual void OnMouseMove(WPARAM btnState, int x, int y) {}

    void UpdateWindowTitle(const GameTimer& timer);

    bool InitMainWindow();
    bool InitDirect3D();
    void CreateCommandObjects();
    void CreateSwapChain();
    void FlushCommandQueue();

    ID3D12Resource* CurrentBackBuffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

protected:
    static DX12Framework* appInstance;

    HINSTANCE appInstanceHandle = nullptr;
    HWND mainWindowHandle = nullptr;
    bool appPaused = false;
    bool minimized = false;
    bool maximized = false;
    bool resizing = false;

    GameTimer timer;

    ComPtr<ID3D12Device> device;
    ComPtr<IDXGIFactory4> factory;
    ComPtr<ID3D12Fence> fence;

    ComPtr<ID3D12CommandQueue> commandQueue;
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;

    ComPtr<IDXGISwapChain3> swapChain;

    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12DescriptorHeap> dsvHeap;

    static const int SwapChainBufferCount = 2;
    ComPtr<ID3D12Resource> swapChainBuffer[SwapChainBufferCount];
    ComPtr<ID3D12Resource> depthStencilBuffer;

    D3D12_VIEWPORT screenViewport;
    D3D12_RECT scissorRect;

    UINT rtvDescriptorSize = 0;
    UINT dsvDescriptorSize = 0;
    UINT cbvSrvUavDescriptorSize = 0;

    std::wstring mainWindowCaption = L"DX12 Framework";
    int clientWidth = 1280;
    int clientHeight = 720;

    DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    UINT64 currentFence = 0;
    int currentBackBuffer = 0;
};

#define ThrowIfFailed(x) \
{ \
    HRESULT hr__ = (x); \
    if(FAILED(hr__)) { \
        throw std::exception("DX12 call failed in file " __FILE__); \
    } \
}