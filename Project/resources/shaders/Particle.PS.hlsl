#include "Particle.hlsli"

struct Material
{
    float4 color;
    float4x4 uvTransform;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4x4 particleUvTransform = float4x4(
        input.uvTransform0,
        input.uvTransform1,
        input.uvTransform2,
        input.uvTransform3);

    // UV変換 & テクスチャサンプリング
    float4 transformedUV = mul(float4(input.texCoord, 0.0f, 1.0f), particleUvTransform);
    transformedUV = mul(transformedUV, gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
     // α=0ならピクセル破棄
    if (textureColor.a == 0.0f)
    {
        discard;
    }
    
    output.color = textureColor * input.color * gMaterial.color;
     
    if (output.color.a == 0.0f)
    {
        discard;
    }
    
    return output;
}
