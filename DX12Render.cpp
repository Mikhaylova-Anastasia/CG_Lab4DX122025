#include "DX12Render.h"
#include <stdexcept>
#include <d3dcompiler.h>
#include <fstream>
#include <iostream>

DX12Render::DX12Render(HWND hwnd, int width, int height) : hwnd(hwnd), width(width), height(height) {
    fenceValue = 0;

    // Инициализируем шейдеры нулями
    vertexShaderBytecode = {};
    pixelShaderBytecode = {};

    InitDevice();
    InitSwapChain(hwnd, width, height);
    CreateRTV();
    WaitForPreviousFrame();

    // Инициализируем пирамиду
    CreatePyramidGeometry();
    CreateConstantBuffers();
    CreateRootSignature();
    LoadShaders();
    CreatePipelineState();
    pyramidInitialized = true;
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

void DX12Render::CreatePyramidGeometry() {
    // Вершины пирамиды
    pyramidVertices = {
        // Основание
        {{-1.0f, 0.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},  // Красный
        {{ 1.0f, 0.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},  // Зеленый
        {{ 1.0f, 0.0f,  1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},  // Синий
        {{-1.0f, 0.0f,  1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}},  // Желтый
        // Вершина
        {{ 0.0f, 2.0f,  0.0f}, {0.0f, 0.894f, 0.447f}, {1.0f, 0.0f, 1.0f}}  // Фиолетовый
    };

    // Индексы
    pyramidIndices = {
        // Основание
        0, 1, 2,
        0, 2, 3,
        // Боковые грани
        0, 4, 1,
        1, 4, 2,
        2, 4, 3,
        3, 4, 0
    };

    // Вершинный буфер
    UINT vertexBufferSize = (UINT)pyramidVertices.size() * sizeof(PyramidVertex);

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = vertexBufferSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer)
    );

    void* vertexData;
    vertexBuffer->Map(0, nullptr, &vertexData);
    memcpy(vertexData, pyramidVertices.data(), vertexBufferSize);
    vertexBuffer->Unmap(0, nullptr);

    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.SizeInBytes = vertexBufferSize;
    vertexBufferView.StrideInBytes = sizeof(PyramidVertex);

    // Индексный буфер
    UINT indexBufferSize = (UINT)pyramidIndices.size() * sizeof(uint16_t);

    bufferDesc.Width = indexBufferSize;

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&indexBuffer)
    );

    void* indexData;
    indexBuffer->Map(0, nullptr, &indexData);
    memcpy(indexData, pyramidIndices.data(), indexBufferSize);
    indexBuffer->Unmap(0, nullptr);

    indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
    indexBufferView.SizeInBytes = indexBufferSize;
    indexBufferView.Format = DXGI_FORMAT_R16_UINT;
}

void DX12Render::CreateConstantBuffers() {
    UINT perObjectCBSize = (sizeof(PerObjectConstants) + 255) & ~255;
    UINT lightCBSize = (sizeof(LightConstants) + 255) & ~255;
    UINT materialCBSize = (sizeof(MaterialConstants) + 255) & ~255;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = perObjectCBSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&perObjectConstantBuffer)
    );

    bufferDesc.Width = lightCBSize;
    device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&lightConstantBuffer)
    );

    bufferDesc.Width = materialCBSize;
    device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&materialConstantBuffer)
    );

    materialCB.ambientColor = XMFLOAT3(0.2f, 0.2f, 0.2f);
    materialCB.diffuseColor = XMFLOAT3(0.8f, 0.8f, 0.8f);
    materialCB.shininess = 64.0f;
    materialCB.specularIntensity = 1.0f;

    lightCB.lightPosition = XMFLOAT3(3.0f, 5.0f, -3.0f);
    lightCB.lightIntensity = 1.5f;
    lightCB.cameraPosition = cameraPosition;
}

void DX12Render::CreateRootSignature() {
    D3D12_ROOT_PARAMETER rootParameters[3] = {};

    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].Descriptor.RegisterSpace = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].Descriptor.ShaderRegister = 1;
    rootParameters[1].Descriptor.RegisterSpace = 0;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].Descriptor.ShaderRegister = 2;
    rootParameters[2].Descriptor.RegisterSpace = 0;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = 3;
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumStaticSamplers = 0;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;

    D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signature,
        &error
    );

    device->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)
    );
}

void DX12Render::LoadShaders() {
    OutputDebugString(L"=== Loading Shaders ===\n");

    const char* vertexShader = R"(
        struct VSInput {
            float3 position : POSITION;
            float3 normal : NORMAL;
            float3 color : COLOR;
        };
        
        struct PSInput {
            float4 position : SV_POSITION;
            float3 normal : NORMAL;
            float3 color : COLOR;
        };
        
        cbuffer PerObject : register(b0) {
            float4x4 world;
            float4x4 view;
            float4x4 projection;
        };
        
        PSInput main(VSInput input) {
            PSInput output;
            
            float4 worldPos = mul(float4(input.position, 1.0f), world);
            float4 viewPos = mul(worldPos, view);
            output.position = mul(viewPos, projection);
            
            output.normal = input.normal;  // Упростим пока без преобразования
            output.color = input.color;
            
            return output;
        }
    )";

    const char* pixelShader = R"(
        struct PSInput {
            float4 position : SV_POSITION;
            float3 normal : NORMAL;
            float3 color : COLOR;
        };
        
        float4 main(PSInput input) : SV_TARGET {
            // Простой шейдер - просто возвращаем цвет
            return float4(input.color, 1.0f);
        }
    )";

    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> psBlob;
    ComPtr<ID3DBlob> errorBlob;

    UINT compileFlags = 0;
#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    OutputDebugString(L"Compiling vertex shader...\n");

    HRESULT hr = D3DCompile(
        vertexShader,
        strlen(vertexShader),
        nullptr,
        nullptr,
        nullptr,
        "main",  // Имя точки входа
        "vs_5_0",
        compileFlags,
        0,
        &vsBlob,
        &errorBlob
    );

    if (FAILED(hr)) {
        std::string errorMsg = "Vertex shader compilation failed: ";
        if (errorBlob) {
            errorMsg += (char*)errorBlob->GetBufferPointer();
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        OutputDebugString(L"Vertex shader FAILED\n");
        throw std::runtime_error(errorMsg.c_str());
    }

    OutputDebugString(L"Vertex shader compiled successfully\n");

    OutputDebugString(L"Compiling pixel shader...\n");

    hr = D3DCompile(
        pixelShader,
        strlen(pixelShader),
        nullptr,
        nullptr,
        nullptr,
        "main",  // Имя точки входа
        "ps_5_0",
        compileFlags,
        0,
        &psBlob,
        &errorBlob
    );

    if (FAILED(hr)) {
        std::string errorMsg = "Pixel shader compilation failed: ";
        if (errorBlob) {
            errorMsg += (char*)errorBlob->GetBufferPointer();
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        OutputDebugString(L"Pixel shader FAILED\n");
        throw std::runtime_error(errorMsg.c_str());
    }

    OutputDebugString(L"Pixel shader compiled successfully\n");

    vertexShaderBytecode.pShaderBytecode = vsBlob->GetBufferPointer();
    vertexShaderBytecode.BytecodeLength = vsBlob->GetBufferSize();

    pixelShaderBytecode.pShaderBytecode = psBlob->GetBufferPointer();
    pixelShaderBytecode.BytecodeLength = psBlob->GetBufferSize();

    OutputDebugString(L"=== Shaders loaded successfully ===\n");
}

void DX12Render::CreatePipelineState() {
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = 3;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = vertexShaderBytecode;
    psoDesc.PS = pixelShaderBytecode;
    psoDesc.InputLayout = inputLayoutDesc;

    psoDesc.RasterizerState.FillMode = wireframeMode ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;

    psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;
    psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
}

void DX12Render::UpdateConstantBuffers(float deltaTime) {
    rotationAngle += deltaTime * 0.3f;

    XMMATRIX world = XMMatrixRotationY(rotationAngle) *
        XMMatrixRotationX(sin(rotationAngle * 0.5f) * 0.1f);
    XMStoreFloat4x4(&perObjectCB.worldMatrix, XMMatrixTranspose(world));

    XMVECTOR eye = XMLoadFloat3(&cameraPosition);
    XMVECTOR at = XMVectorSet(0.0f, 0.5f, 0.0f, 0.0f);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
    XMStoreFloat4x4(&perObjectCB.viewMatrix, XMMatrixTranspose(view));

    float aspectRatio = (float)width / (float)height;
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspectRatio, 0.1f, 100.0f);
    XMStoreFloat4x4(&perObjectCB.projectionMatrix, XMMatrixTranspose(proj));

    lightCB.cameraPosition = cameraPosition;

    void* mappedData;

    perObjectConstantBuffer->Map(0, nullptr, &mappedData);
    memcpy(mappedData, &perObjectCB, sizeof(perObjectCB));
    perObjectConstantBuffer->Unmap(0, nullptr);

    lightConstantBuffer->Map(0, nullptr, &mappedData);
    memcpy(mappedData, &lightCB, sizeof(lightCB));
    lightConstantBuffer->Unmap(0, nullptr);

    materialConstantBuffer->Map(0, nullptr, &mappedData);
    memcpy(mappedData, &materialCB, sizeof(materialCB));
    materialConstantBuffer->Unmap(0, nullptr);
}

void DX12Render::Update(float deltaTime) {
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
        static bool spacePressed = false;
        if (!spacePressed) {
            wireframeMode = !wireframeMode;
            CreatePipelineState();
            spacePressed = true;
        }
    }
    else {
        static bool spacePressed = false;
        spacePressed = false;
    }

    UpdateConstantBuffers(deltaTime);
}

void DX12Render::DrawPyramid() {
    cmdAllocator->Reset();
    cmdList->Reset(cmdAllocator.Get(), pipelineState.Get());

    cmdList->SetGraphicsRootSignature(rootSignature.Get());
    cmdList->SetGraphicsRootConstantBufferView(0, perObjectConstantBuffer->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootConstantBufferView(1, lightConstantBuffer->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootConstantBufferView(2, materialConstantBuffer->GetGPUVirtualAddress());

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);
    cmdList->IASetIndexBuffer(&indexBufferView);

    cmdList->DrawIndexedInstanced((UINT)pyramidIndices.size(), 1, 0, 0, 0);
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

    if (pyramidInitialized) {
        DrawPyramid();
    }

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
        clearColor[0] = 0.05f;
        clearColor[1] = 0.05f;
        clearColor[2] = 0.1f;
        break;
    }
}

void DX12Render::SetText(const std::wstring& text) {
    displayText = text;
}