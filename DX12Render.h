#pragma once
#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <string>
#include <vector>
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

struct PyramidVertex {
    float position[3];
    float normal[3];
    float color[3];
};

struct PerObjectConstants {
    XMFLOAT4X4 worldMatrix;
    XMFLOAT4X4 viewMatrix;
    XMFLOAT4X4 projectionMatrix;
};

struct LightConstants {
    XMFLOAT3 lightPosition;
    float lightIntensity;
    XMFLOAT3 cameraPosition;
    float padding;
};

struct MaterialConstants {
    XMFLOAT3 ambientColor;
    float shininess;
    XMFLOAT3 diffuseColor;
    float specularIntensity;
};

class DX12Render {
public:
    DX12Render(HWND hwnd, int width, int height);
    ~DX12Render();

    void Render();
    void HandleKeyInput(WPARAM key);
    void SetText(const std::wstring& text);
    void Update(float deltaTime);

private:
    void InitDevice();
    void InitSwapChain(HWND hwnd, int width, int height);
    void CreateRTV();
    void WaitForPreviousFrame();

    void CreatePyramidGeometry();
    void CreateConstantBuffers();
    void CreateRootSignature();
    void CreatePipelineState();
    void LoadShaders();
    void UpdateConstantBuffers(float deltaTime);
    void DrawPyramid();

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

    // Ресурсы для пирамиды
    std::vector<PyramidVertex> pyramidVertices;
    std::vector<uint16_t> pyramidIndices;

    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;

    ComPtr<ID3D12Resource> perObjectConstantBuffer;
    ComPtr<ID3D12Resource> lightConstantBuffer;
    ComPtr<ID3D12Resource> materialConstantBuffer;

    PerObjectConstants perObjectCB;
    LightConstants lightCB;
    MaterialConstants materialCB;

    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pipelineState;

    D3D12_SHADER_BYTECODE vertexShaderBytecode = {};
    D3D12_SHADER_BYTECODE pixelShaderBytecode = {};

    HWND hwnd;
    int width, height;

    float clearColor[4] = { 0.05f, 0.05f, 0.1f, 1.0f };
    std::wstring displayText = L"DX12 Pyramid";
    float rotationAngle = 0.0f;
    XMFLOAT3 cameraPosition = { 0.0f, 2.0f, -5.0f };
    bool wireframeMode = false;
    bool pyramidInitialized = false;
};