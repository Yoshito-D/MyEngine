#include "FullscreenTriangle.hlsli"
cbuffer VignetteParams : register(b0)
{
    float2 center; // UV空間での中心（通常は0.5,0.5）
    float radius; // ビネットの半径（0〜1）
    float softness; // ぼかしの強さ（0〜1）
    float3 vignetteColor;
    float intensity; // ビネットの強度（暗さの最大値）
};

Texture2D inputTexture : register(t0);
SamplerState samplerLinear : register(s0);

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    float4 color = inputTexture.Sample(samplerLinear, uv);

    float2 diff = uv - center;
    float dist = length(diff);

    // ビネット係数を計算（0〜1の範囲）
    float vignette = smoothstep(radius - softness, radius, dist);

    // ビネット効果をかける（暗くする）
    color.rgb = lerp(color.rgb, vignetteColor, vignette * intensity);

    return color;
}
