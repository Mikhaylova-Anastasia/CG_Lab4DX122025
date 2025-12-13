cbuffer ObjectCB : register(b0)
{
    float4x4 gWorldViewProj;
    float4x4 gWorld;

    float3 gEyePosW;
    float pad0;

    float4 gLightDir; // (xyz) направление света
    float4 gDiffuseColor; // цвет диффуза
    float4 gSpecularColor; // цвет блика
    float gShininess; // степень блеска
    float3 pad1;
};

struct VSInput
{
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
};

struct VSOutput
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
};

VSOutput VSMain(VSInput vin)
{
    VSOutput vout;

    float4 posW = mul(float4(vin.Pos, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    vout.PosH = mul(posW, gWorldViewProj);

    float3 nW = mul(float4(vin.Normal, 0.0f), gWorld).xyz;
    vout.NormalW = normalize(nW);

    return vout;
}