// CubeRenderer.cpp
#include "CubeRenderer.h"

struct VertexPosNormal
{
    XMFLOAT3 Pos;
    XMFLOAT3 Normal;
};

CubeRenderer::CubeRenderer(ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    UINT cbvSrvUavDescriptorSize)
    : mDevice(device)
    , mCmdList(cmdList)
    , mCbvSrvUavDescriptorSize(cbvSrvUavDescriptorSize)
{
    
    float aspect = 1280.0f / 720.0f;
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * XM_PI, aspect, 0.1f, 100.0f);
    XMStoreFloat4x4(&mProj, P);
}

void CubeRenderer::BuildResources()
{
    BuildCubeGeometry();
    BuildConstantBuffer();
    BuildRootSignature();
    BuildPSO();
}

void CubeRenderer::BuildCubeGeometry()
{
    VertexPosNormal vertices[] =
    {
        // Front (-Z)
        {{-1,-1,-1},{0,0,-1}}, {{-1, 1,-1},{0,0,-1}}, {{ 1, 1,-1},{0,0,-1}}, {{ 1,-1,-1},{0,0,-1}},
        // Back (+Z)
        {{-1,-1, 1},{0,0, 1}}, {{ 1,-1, 1},{0,0, 1}}, {{ 1, 1, 1},{0,0, 1}}, {{-1, 1, 1},{0,0, 1}},
        // Left (-X)
        {{-1,-1, 1},{-1,0,0}}, {{-1, 1, 1},{-1,0,0}}, {{-1, 1,-1},{-1,0,0}}, {{-1,-1,-1},{-1,0,0}},
        // Right (+X)
        {{ 1,-1,-1},{ 1,0,0}}, {{ 1, 1,-1},{ 1,0,0}}, {{ 1, 1, 1},{ 1,0,0}}, {{ 1,-1, 1},{ 1,0,0}},
        // Top (+Y)
        {{-1, 1,-1},{0, 1,0}}, {{-1, 1, 1},{0, 1,0}}, {{ 1, 1, 1},{0, 1,0}}, {{ 1, 1,-1},{0, 1,0}},
        // Bottom (-Y)
        {{-1,-1, 1},{0,-1,0}}, {{-1,-1,-1},{0,-1,0}}, {{ 1,-1,-1},{0,-1,0}}, {{ 1,-1, 1},{0,-1,0}},
    };

    uint16_t indices[] =
    {
        0,1,2, 0,2,3,       // front
        4,5,6, 4,6,7,       // back
        8,9,10, 8,10,11,    // left
        12,13,14, 12,14,15, // right
        16,17,18, 16,18,19, // top
        20,21,22, 20,22,23  // bottom
    };

    mIndexCount = _countof(indices);

    UINT vBufferSize = sizeof(vertices);
    UINT iBufferSize = sizeof(indices);

    CD3DX12_HEAP_PROPERTIES heapPropsVB(D3D12_HEAP_TYPE_UPLOAD);
    auto vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vBufferSize);

    ThrowIfFailed(mDevice->CreateCommittedResource(
        &heapPropsVB, D3D12_HEAP_FLAG_NONE, &vbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&mVertexBuffer)));

    CD3DX12_HEAP_PROPERTIES heapPropsIB(D3D12_HEAP_TYPE_UPLOAD);
    auto ibDesc = CD3DX12_RESOURCE_DESC::Buffer(iBufferSize);

    ThrowIfFailed(mDevice->CreateCommittedResource(
        &heapPropsIB, D3D12_HEAP_FLAG_NONE, &ibDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&mIndexBuffer)));

    void* mapped = nullptr;
    ThrowIfFailed(mVertexBuffer->Map(0, nullptr, &mapped));
    memcpy(mapped, vertices, vBufferSize);
    mVertexBuffer->Unmap(0, nullptr);

    ThrowIfFailed(mIndexBuffer->Map(0, nullptr, &mapped));
    memcpy(mapped, indices, iBufferSize);
    mIndexBuffer->Unmap(0, nullptr);

    mVBV.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
    mVBV.StrideInBytes = sizeof(VertexPosNormal);
    mVBV.SizeInBytes = vBufferSize;

    mIBV.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
    mIBV.Format = DXGI_FORMAT_R16_UINT;
    mIBV.SizeInBytes = iBufferSize;
}

void CubeRenderer::BuildConstantBuffer()
{
    UINT cbSize = (sizeof(ObjectConstants) + 255) & ~255; 

    CD3DX12_HEAP_PROPERTIES heapPropsCB(D3D12_HEAP_TYPE_UPLOAD);
    auto cbDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

    ThrowIfFailed(mDevice->CreateCommittedResource(
        &heapPropsCB,
        D3D12_HEAP_FLAG_NONE,
        &cbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mConstantUploadBuffer)));

    mConstantBuffer = mConstantUploadBuffer;
}

void CubeRenderer::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER rootParam;
    rootParam.InitAsConstantBufferView(0); // b0

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
        1,
        &rootParam,
        0,
        nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig;
    ComPtr<ID3DBlob> errorBlob;

    ThrowIfFailed(D3D12SerializeRootSignature(
        &rootSigDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &serializedRootSig,
        &errorBlob));

    ThrowIfFailed(mDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&mRootSignature)));
}

void CubeRenderer::BuildPSO()
{
    ComPtr<ID3DBlob> vs;
    ComPtr<ID3DBlob> ps;
    ComPtr<ID3DBlob> errors;

    // VS
    HRESULT hr = D3DCompileFromFile(
        L"Shaders/CubeVS.hlsl",
        nullptr, nullptr,
        "VSMain", "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &vs, &errors);

    if (FAILED(hr))
    {
        if (errors)
            MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "VS compile error", MB_OK);
        ThrowIfFailed(hr);
    }

    // PS
    hr = D3DCompileFromFile(
        L"Shaders/CubePS.hlsl",
        nullptr, nullptr,
        "PSMain", "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &ps, &errors);

    if (FAILED(hr))
    {
        if (errors)
            MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "PS compile error", MB_OK);
        ThrowIfFailed(hr);
    }

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };

    CD3DX12_RASTERIZER_DESC rast(D3D12_DEFAULT);
    rast.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState = rast;

    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc.Count = 1;

    ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}

void CubeRenderer::UpdateCubeRotation(const InputDevice& input, float /*dt*/)
{
    
    if (input.IsMouseDown(0))
    {
        const float rotSpeed = 0.01f;

        POINT md = input.MouseDelta();
        mCubeYaw += md.x * rotSpeed;
        mCubePitch += md.y * rotSpeed;

        
        const float limit = XM_PIDIV2 - 0.01f;
        if (mCubePitch > limit) mCubePitch = limit;
        if (mCubePitch < -limit) mCubePitch = -limit;
    }
}

void CubeRenderer::UpdateCamera(const InputDevice& input, float dt)
{
    const float moveSpeed = 5.0f;
    const float mouseSens = 0.0025f;

    
    if (input.IsMouseDown(1))
    {
        POINT md = input.MouseDelta();
        mYaw += md.x * mouseSens;
        mPitch += md.y * mouseSens;

        
        const float limit = XM_PIDIV2 - 0.1f;
        if (mPitch > limit) mPitch = limit;
        if (mPitch < -limit) mPitch = -limit;
    }

    XMVECTOR forward = XMVectorSet(
        cosf(mPitch) * sinf(mYaw),
        sinf(mPitch),
        cosf(mPitch) * cosf(mYaw),
        0.0f);

    XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));

    XMVECTOR pos = XMLoadFloat3(&mCameraPos);

    if (input.IsKeyDown('W')) pos += forward * moveSpeed * dt;
    if (input.IsKeyDown('S')) pos -= forward * moveSpeed * dt;
    if (input.IsKeyDown('A')) pos -= right * moveSpeed * dt;
    if (input.IsKeyDown('D')) pos += right * moveSpeed * dt;

    XMStoreFloat3(&mCameraPos, pos);
}

void CubeRenderer::Update(float /*totalTime*/, float deltaTime, const InputDevice& input)
{
    
    UpdateCubeRotation(input, deltaTime);
    UpdateCamera(input, deltaTime);

    
    XMMATRIX world =
        XMMatrixRotationX(mCubePitch) *
        XMMatrixRotationY(mCubeYaw);

    
    XMVECTOR pos = XMLoadFloat3(&mCameraPos);
    XMVECTOR forward = XMVectorSet(
        cosf(mPitch) * sinf(mYaw),
        sinf(mPitch),
        cosf(mPitch) * cosf(mYaw),
        0.0f);

    XMMATRIX view = XMMatrixLookAtLH(
        pos,
        pos + forward,
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    XMMATRIX proj = XMLoadFloat4x4(&mProj);
    XMMATRIX wvp = world * view * proj;

    XMStoreFloat4x4(&mConstants.WorldViewProj, XMMatrixTranspose(wvp));
    XMStoreFloat4x4(&mConstants.World, XMMatrixTranspose(world));

    
    mConstants.EyePosW = mCameraPos;

    
    mConstants.LightDir = XMFLOAT4(0.5f, -1.0f, -0.3f, 0.0f);
    mConstants.DiffuseColor = XMFLOAT4(0.0f, 0.8f, 0.2f, 1.0f);
    mConstants.SpecularColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    mConstants.Shininess = 16.0f;

    
    void* mapped = nullptr;
    ThrowIfFailed(mConstantUploadBuffer->Map(0, nullptr, &mapped));
    memcpy(mapped, &mConstants, sizeof(ObjectConstants));
    mConstantUploadBuffer->Unmap(0, nullptr);
}

void CubeRenderer::Draw(ID3D12GraphicsCommandList* cmdList)
{
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &mVBV);
    cmdList->IASetIndexBuffer(&mIBV);

    cmdList->SetGraphicsRootConstantBufferView(
        0,
        mConstantUploadBuffer->GetGPUVirtualAddress());

    cmdList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
}
