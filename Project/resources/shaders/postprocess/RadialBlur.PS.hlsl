#include "FullscreenTriangle.hlsli"

cbuffer RadialBlurCB : register(b0)
{
    float32_t2 gCenter;
    float gStrength;
    int32_t gSampleCount;
};

Texture2D gInputTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VSOutput input) : SV_TARGET
{
    float32_t2 uv = input.texcoord;

    // 中心からの方向
    float32_t2 dir = gCenter - uv;

    // 放射状にサンプル
    float32_t4 color = float32_t4(0, 0, 0, 0);
    for (int32_t i = 0; i < gSampleCount; ++i)
    {
        float t = (float) i / (float) (gSampleCount - 1);
        float32_t2 sampleUV = uv + dir * gStrength * t;
        color += gInputTexture.Sample(gSampler, sampleUV);
    }

    color /= gSampleCount;
    return color;
}