#include "LightingUtil.hlsl"

Texture2D gDiffuseMap : register(t0);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
};

cbuffer cbMaterial : register(b1)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
};

cbuffer cbPass : register(b2)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

struct VertexIn
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 Normal : NORMAL;
    float2 TexC : TEXCOORD;
};

float4 main(VertexIn vin) : SV_TARGET
{
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, vin.TexC) * gDiffuseAlbedo;
    
    vin.Normal = normalize(vin.Normal);
    
    float3 toEyeW = normalize(gEyePosW - vin.PosW); // v

    const float shininess = 1.f - gRoughness;
    Material mat = { diffuseAlbedo, gFresnelR0, shininess };
    float3 shadowFactor = 1.f;
    
    // Diffuse and Specular Light
    float4 directLight = ComputeLighting(gLights, mat, vin.PosW, vin.Normal, toEyeW, shadowFactor);
    
    // Ambient Light
    float4 ambient = gAmbientLight * diffuseAlbedo; // Al x md
    
    float4 litColor = ambient + directLight;

    clip(diffuseAlbedo.a - 0.1f);
    
    litColor.a = diffuseAlbedo.a;
    
    return litColor;
}