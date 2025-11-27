#include "DX12Render.h"
#include <stdexcept>

DX12Render::DX12Render(HWND hwnd, int width, int height) : hwnd(hwnd), width(width), height(height) {
    fenceValue = 0;
    InitDevice();
    InitSwapChain(hwnd, width, height);
    CreateRTV();
    WaitForPreviousFrame();
}

DX12Render::~DX12Render() {
    if (fenceEvent) {
        CloseHandle(fenceEvent);
    }
}

void DX12Render::InitDevice() {
    HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(hr)) throw std::runtime_error("D3D12CreateDevice failed");

    D3D12_COMMAND_QUEUE_DESC qDesc = {};
    qDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(&cmdQueue));
    if (FAILED(hr)) throw std::runtime_error("CreateCommandQueue failed");

    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAllocator));
    if (FAILED(hr)) throw std::runtime_error("CreateCommandAllocator failed");

    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocator.Get(), nullptr, IID_PPV_ARGS(&cmdList));
    if (FAILED(hr)) throw std::runtime_error("CreateCommandList failed");

    cmdList->Close();

    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (FAILED(hr)) throw std::runtime_error("CreateFence failed");

    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent) throw std::runtime_error("CreateEvent failed");
}

void DX12Render::InitD2D() {
    // Пустая реализация - убрали сложную D2D интеграцию
}

void DX12Render::InitSwapChain(HWND hwnd, int width, int height) {
    ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) throw std::runtime_error("CreateDXGIFactory1 failed");

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.BufferCount = FrameCount;
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> tempChain;
    hr = factory->CreateSwapChainForHwnd(cmdQueue.Get(), hwnd, &desc, nullptr, nullptr, &tempChain);
    if (FAILED(hr)) throw std::runtime_error("CreateSwapChainForHwnd failed");

    hr = tempChain.As(&swapChain);
    if (FAILED(hr)) throw std::runtime_error("Query IDXGISwapChain3 failed");

    frameIndex = swapChain->GetCurrentBackBufferIndex();
}

void DX12Render::CreateRTV() {
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = FrameCount;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap));
    if (FAILED(hr)) throw std::runtime_error("CreateDescriptorHeap failed");

    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < FrameCount; i++) {
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
        if (FAILED(hr)) throw std::runtime_error("GetBuffer failed");
        device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, handle);
        handle.ptr += rtvDescriptorSize;
    }
}

void DX12Render::WaitForPreviousFrame() {
    const UINT64 signalValue = fenceValue++;
    HRESULT hr = cmdQueue->Signal(fence.Get(), signalValue);
    if (FAILED(hr)) throw std::runtime_error("Signal failed");

    if (fence->GetCompletedValue() < signalValue) {
        hr = fence->SetEventOnCompletion(signalValue, fenceEvent);
        if (FAILED(hr)) throw std::runtime_error("SetEventOnCompletion failed");
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    frameIndex = swapChain->GetCurrentBackBufferIndex();
}

void DX12Render::Render() {
    cmdAllocator->Reset();
    cmdList->Reset(cmdAllocator.Get(), nullptr);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += rtvDescriptorSize * frameIndex;

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = renderTargets[frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    D3D12_RESOURCE_BARRIER barrier2 = {};
    barrier2.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier2.Transition.pResource = renderTargets[frameIndex].Get();
    barrier2.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier2.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier2.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier2);

    cmdList->Close();

    ID3D12CommandList* lists[] = { cmdList.Get() };
    cmdQueue->ExecuteCommandLists(1, lists);

    swapChain->Present(1, 0);
    WaitForPreviousFrame();
}

void DX12Render::HandleKeyInput(WPARAM key) {
    switch (key) {
    case '1':
        clearColor[0] = 1.0f;
        clearColor[1] = 0.0f;
        clearColor[2] = 0.0f;
        break;
    case '2':
        clearColor[0] = 0.0f;
        clearColor[1] = 1.0f;
        clearColor[2] = 0.0f;
        break;
    case '3':
        clearColor[0] = 0.0f;
        clearColor[1] = 0.0f;
        clearColor[2] = 1.0f;
        break;
    case '0':
        clearColor[0] = 0.0f;
        clearColor[1] = 0.2f;
        clearColor[2] = 0.4f;
        break;
    }
}

void DX12Render::SetText(const std::wstring& text) {
    displayText = text;
}