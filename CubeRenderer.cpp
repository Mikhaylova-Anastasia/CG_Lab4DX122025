// CubeRenderer.cpp
#include "CubeRenderer.h"
#include "ObjLoader.h"

CubeRenderer::CubeRenderer(ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    UINT cbvSrvUavDescriptorSize)
    : mDevice(device)
    , mCmdList(cmdList)
    , mCbvSrvUavDescriptorSize(cbvSrvUavDescriptorSize)
{
    float aspect = 1280.0f / 720.0f;
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * XM_PI, aspect, 0.1f, 5000.0f);
    XMStoreFloat4x4(&mProj, P);

    
    mCameraPos = XMFLOAT3(0.0f, 2.0f, -5.0f);
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
    ObjMeshData mesh;

    
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *(lastSlash + 1) = 0;

    std::wstring objPath = std::wstring(exePath) + L"Models\\sponza.obj";

    if (!ObjLoader::LoadObjPosNormal(objPath, mesh, true))
    {
        MessageBoxW(nullptr, objPath.c_str(), L"OBJ NOT FOUND AT:", MB_OK | MB_ICONERROR);
        throw std::runtime_error("OBJ load failed");
    }

    mIndexCount = (UINT)mesh.Indices.size();

    const UINT vBufferSize = (UINT)(mesh.Vertices.size() * sizeof(VertexPosNormal));
    const UINT iBufferSize = (UINT)(mesh.Indices.size() * sizeof(uint32_t));

   
    CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);

    CD3DX12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vBufferSize);
    CD3DX12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC::Buffer(iBufferSize);

    
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &vbDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&mVertexBuffer)));

    
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &vbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mVBUpload)));

    void* mapped = nullptr;
    ThrowIfFailed(mVBUpload->Map(0, nullptr, &mapped));
    memcpy(mapped, mesh.Vertices.data(), vBufferSize);
    mVBUpload->Unmap(0, nullptr);

    mCmdList->CopyBufferRegion(mVertexBuffer.Get(), 0, mVBUpload.Get(), 0, vBufferSize);
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            mVertexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        mCmdList->ResourceBarrier(1, &barrier);
    }

    
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &ibDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&mIndexBuffer)));

    
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &ibDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mIBUpload)));

    ThrowIfFailed(mIBUpload->Map(0, nullptr, &mapped));
    memcpy(mapped, mesh.Indices.data(), iBufferSize);
    mIBUpload->Unmap(0, nullptr);

    mCmdList->CopyBufferRegion(mIndexBuffer.Get(), 0, mIBUpload.Get(), 0, iBufferSize);
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            mIndexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_INDEX_BUFFER);
        mCmdList->ResourceBarrier(1, &barrier);
    }

    mVBV.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
    mVBV.StrideInBytes = sizeof(VertexPosNormal);
    mVBV.SizeInBytes = vBufferSize;

    mIBV.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
    mIBV.Format = DXGI_FORMAT_R32_UINT;
    mIBV.SizeInBytes = iBufferSize;

    
    if (mIndexCount < 1000)
        MessageBoxW(nullptr, L"Warning: too few indices, is this really Sponza?", L"DBG", MB_OK);
}

void CubeRenderer::BuildConstantBuffer()
{
    UINT cbSize = (sizeof(ObjectConstants) + 255) & ~255;

    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC cbDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

    ThrowIfFailed(mDevice->CreateCommittedResource(
        &uploadHeapProps,
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
    rootParam.InitAsConstantBufferView(0);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
        1, &rootParam,
        0, nullptr,
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

    HRESULT hr = D3DCompileFromFile(
        L"Shaders/CubeVS.hlsl",
        nullptr, nullptr,
        "VSMain", "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0, &vs, &errors);

    if (FAILED(hr))
    {
        if (errors) MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "VS compile error", MB_OK);
        ThrowIfFailed(hr);
    }

    hr = D3DCompileFromFile(
        L"Shaders/CubePS.hlsl",
        nullptr, nullptr,
        "PSMain", "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0, &ps, &errors);

    if (FAILED(hr))
    {
        if (errors) MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "PS compile error", MB_OK);
        ThrowIfFailed(hr);
    }

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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

void CubeRenderer::UpdateCubeRotation(const InputDevice& input, float)
{
   
    if (input.IsMouseDown(0))
    {
        const float rotSpeed = 0.01f;
        POINT md = input.MouseDelta();
        mCubeYaw += md.x * rotSpeed;
        mCubePitch += md.y * rotSpeed;

        const float limit = XM_PIDIV2 - 0.01f;
        if (mCubePitch > limit)  mCubePitch = limit;
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
        if (mPitch > limit)  mPitch = limit;
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

void CubeRenderer::Update(float, float deltaTime, const InputDevice& input)
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

    XMMATRIX view = XMMatrixLookAtLH(pos, pos + forward, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    XMMATRIX proj = XMLoadFloat4x4(&mProj);
    XMMATRIX wvp = world * view * proj;

    XMStoreFloat4x4(&mConstants.WorldViewProj, XMMatrixTranspose(wvp));
    XMStoreFloat4x4(&mConstants.World, XMMatrixTranspose(world));

    mConstants.EyePosW = mCameraPos;

    mConstants.LightDir = XMFLOAT4(0.5f, -1.0f, -0.3f, 0.0f);
    mConstants.DiffuseColor = XMFLOAT4(0.0f, 0.8f, 0.2f, 1.0f);
    mConstants.SpecularColor = XMFLOAT4(1, 1, 1, 1);
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

    cmdList->SetGraphicsRootConstantBufferView(0, mConstantUploadBuffer->GetGPUVirtualAddress());
    cmdList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
}
