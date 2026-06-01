#include "FullscreenTriangle.hlsli"

Texture2D gInputTexture : register(t0);
SamplerState gSampler : register(s0);

// リニア空間からsRGB空間への変換
float3 LinearToSRGB(float3 color)
{
    float3 low  = color * 12.92f;
    float3 high = 1.055f * pow(color, 1.0f / 2.4f) - 0.055f;
    return lerp(high, low, step(color, 0.0031308f));
}

float4 main(VSOutput input) : SV_TARGET
{
    // 入力テクスチャからサンプリング（リニア空間想定）
    float4 color = gInputTexture.Sample(gSampler, input.texcoord);

    // リニア空間からsRGB空間に変換
    color.rgb = LinearToSRGB(color.rgb);

    return color;
}
