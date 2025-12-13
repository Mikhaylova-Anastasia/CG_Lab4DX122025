// CubeRenderer.h
#pragma once
#include "Common.h"
#include "InputDevice.h"

struct ObjectConstants
{
    XMFLOAT4X4 WorldViewProj;
    XMFLOAT4X4 World;

    XMFLOAT3   EyePosW;
    float      pad0;

    XMFLOAT4   LightDir;
    XMFLOAT4   DiffuseColor;

    XMFLOAT4   SpecularColor;
    float      Shininess;
    XMFLOAT3   pad1;
};

class CubeRenderer
{
public:
    CubeRenderer(ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        UINT cbvSrvUavDescriptorSize);

    void BuildResources();
    void BuildPSO();

    void Update(float totalTime, float deltaTime, const InputDevice& input);
    void Draw(ID3D12GraphicsCommandList* cmdList);

    ID3D12RootSignature* GetRootSignature() const { return mRootSignature.Get(); }
    ID3D12PipelineState* GetPSO() const { return mPSO.Get(); }

private:
    void BuildCubeGeometry();
    void BuildConstantBuffer();
    void BuildRootSignature();

    void UpdateCamera(const InputDevice& input, float dt);
    void UpdateCubeRotation(const InputDevice& input, float dt);

private:
    ID3D12Device* mDevice;
    ID3D12GraphicsCommandList* mCmdList;

    UINT mCbvSrvUavDescriptorSize;

    ComPtr<ID3D12Resource> mVertexBuffer;
    ComPtr<ID3D12Resource> mIndexBuffer;
    ComPtr<ID3D12Resource> mConstantBuffer;

    ComPtr<ID3D12Resource> mConstantUploadBuffer;

    D3D12_VERTEX_BUFFER_VIEW mVBV = {};
    D3D12_INDEX_BUFFER_VIEW  mIBV = {};

    UINT mIndexCount = 0;

    ObjectConstants mConstants;

    ComPtr<ID3D12RootSignature> mRootSignature;
    ComPtr<ID3D12PipelineState> mPSO;

    XMFLOAT4X4 mProj;

    
    XMFLOAT3 mCameraPos = { 0.0f, 2.0f, -5.0f };
    float mYaw = 0.0f;
    float mPitch = 0.0f;

   
    float mCubeYaw = 0.0f;
    float mCubePitch = 0.0f;
};
