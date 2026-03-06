#include "Particle.hlsli"

struct Material
{
    float32_t4 color;
    float32_t4x4 uvTransform;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV変換 & テクスチャサンプリング
    float32_t4 transformedUV = mul(float32_t4(input.texCoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
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
