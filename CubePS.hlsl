

cbuffer ObjectCB : register(b0)
{
    float4x4 gWorldViewProj;
    float4x4 gWorld;

    float3 gEyePosW;
    float pad0;

    float4 gLightDir; 
    float4 gDiffuseColor; 
    float4 gSpecularColor;
    float gShininess; 
    float3 pad1;
};

struct PSInput
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
};

float4 PSMain(PSInput pin) : SV_Target
{
    float3 N = normalize(pin.NormalW);

    
    float3 L = normalize(-gLightDir.xyz);

    
    float3 V = normalize(gEyePosW - pin.PosW);

    
    float3 H = normalize(L + V);

  
    float NdotL = saturate(dot(N, L));
    float3 diffuse = gDiffuseColor.rgb * NdotL;

    
    float3 specular = 0.0f;
    if (NdotL > 0.0f)
    {
        float specFactor = pow(saturate(dot(N, H)), gShininess);
        specular = gSpecularColor.rgb * specFactor;
    }

    
    float3 ambient = 0.2f * gDiffuseColor.rgb;

    float3 color = ambient + diffuse + specular;
    return float4(color, 1.0f);
}
