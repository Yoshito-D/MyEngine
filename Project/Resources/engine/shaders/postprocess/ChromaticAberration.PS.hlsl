#include "FullscreenTriangle.hlsli"

Texture2D sceneTex : register(t0);
SamplerState samLinear : register(s0);

cbuffer ParamsCB : register(b0)
{
    float2 center; // 中心UV
    float pixelShift;
    int32_t useFixedDirection; // 0 or 1
    float2 fixedDirection; // 固定方向
    float2 texSize;
};

float4 main(VSOutput input) : SV_TARGET
{
    float2 dir;
    float2 shiftUV;

    if (useFixedDirection != 0)
    {
        dir = normalize(fixedDirection);
        shiftUV = dir * (pixelShift / texSize); // ピクセル→UV変換
    }
    else
    {
        dir = normalize(input.texcoord - center);
        float dist = length(input.texcoord - center);
        shiftUV = dir * (pixelShift / texSize) * dist;
    }


    float r = sceneTex.Sample(samLinear, input.texcoord + shiftUV).r;
    float g = sceneTex.Sample(samLinear, input.texcoord).g;
    float b = sceneTex.Sample(samLinear, input.texcoord - shiftUV).b;
    float a = sceneTex.Sample(samLinear, input.texcoord).a;
    
    return float4(r, g, b, a);
}
