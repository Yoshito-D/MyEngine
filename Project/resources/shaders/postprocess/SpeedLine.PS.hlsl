#include "FullscreenTriangle.hlsli"

cbuffer SpeedLineCB : register(b0)
{
    float2 gCenter;
    float gIntensity;
    float gLineDensity;
    float gThickness;
    float gInnerRadius;
    float gOuterRadius;
    float gTime;
    float gRandomSeed;
    float gFlowSpeed;
};

Texture2D gInputTexture : register(t0);
SamplerState gSampler : register(s0);

static const float PI = 3.14159265f;

float Hash(float n)
{
    return frac(sin(n) * 43758.5453123f);
}

float4 main(VSOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float4 baseColor = gInputTexture.Sample(gSampler, uv);

    float2 dir = uv - gCenter;
    float dist = length(dir);

    if (dist < 0.0001f)
    {
        return baseColor;
    }

    float angle = atan2(dir.y, dir.x);

    float sector = floor((angle + PI) * gLineDensity);
    float rnd = Hash(sector + gRandomSeed);
    float stripe = step(gThickness, rnd);

    float lengthRnd = Hash(sector * 7.13f + 123.4f);

    float wave = sin(angle * 12.0f + lengthRnd * 20.0f + gTime * 2.0f);
    wave = wave * 0.5f + 0.5f;

    float lineLength = lerp(gInnerRadius, gOuterRadius, lengthRnd);
    lineLength *= lerp(0.7f, 1.3f, wave);

    // ★重要：順序破綻防止（innerRadiusより小さくならないようにする）
    lineLength = max(lineLength, gInnerRadius + 0.001f);

    // ★内側を完全にカット
    float innerMask = step(gInnerRadius, dist);

    float radialMask = smoothstep(gInnerRadius, lineLength, dist);

    float tipFade = 1.0f - smoothstep(lineLength * 0.7f, lineLength, dist);
    radialMask *= tipFade;

    float flow = sin(dist * 15.0f - gTime * gFlowSpeed);
    flow = flow * 0.5f + 0.5f;

    float lineMask = stripe * radialMask * flow * gIntensity;

    lineMask *= innerMask;

    float3 result = baseColor.rgb;
    result += lineMask;

    return float4(saturate(result), baseColor.a);
}