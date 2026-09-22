#include "Object3d.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// The JSON contract checks the type, byte offset and total cbuffer size against DXC reflection.
cbuffer MaterialParameters : register(b1) {
    float4 tint;
    float intensity;
};

float4 main(VertexShaderOutput input) : SV_TARGET0 {
    float2 uv = mul(float4(input.texCoord, 0, 1), gMaterial.uvTransform).xy;
    float4 color = gTexture.Sample(gSampler, uv) * gMaterial.color * tint;
    color.rgb *= intensity;
    return color;
}
