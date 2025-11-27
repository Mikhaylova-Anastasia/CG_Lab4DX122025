#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <string>

using Microsoft::WRL::ComPtr;

class DX12Render {
public:
    DX12Render(HWND hwnd, int width, int height);
    ~DX12Render();

    void Render();
    void HandleKeyInput(WPARAM key);
    void SetText(const std::wstring& text);

private:
    void InitDevice();
    void InitSwapChain(HWND hwnd, int width, int height);
    void CreateRTV();
    void WaitForPreviousFrame();
    void InitD2D();

private:
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> cmdQueue;
    ComPtr<IDXGISwapChain3> swapChain;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12CommandAllocator> cmdAllocator;
    ComPtr<ID3D12GraphicsCommandList> cmdList;
    ComPtr<ID3D12Fence> fence;
    UINT frameIndex = 0;
    UINT rtvDescriptorSize = 0;
    HANDLE fenceEvent = nullptr;
    UINT64 fenceValue = 0;
    static const UINT FrameCount = 2;
    ComPtr<ID3D12Resource> renderTargets[FrameCount];

    HWND hwnd;
    int width, height;

    float clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
    std::wstring displayText = L"Type text here...";
};