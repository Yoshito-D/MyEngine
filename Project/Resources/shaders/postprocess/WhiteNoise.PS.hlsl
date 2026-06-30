#include "FullscreenTriangle.hlsli"

cbuffer WhiteNoiseCB : register(b0)
{
    float gTime;
    float gNoiseDensity;
    float gSeedChangeRate;
    float gNoiseThreshold;
    float gNoiseIntensity;
};

Texture2D gInputTexture : register(t0);
SamplerState gSampler : register(s0);

float rand2dTo1d(float2 value, float2 dotDir = float2(12.9898f, 78.233f))
{
    float2 smallValue = sin(value);
    float random = dot(smallValue, dotDir);
    random = frac(sin(random) * 143758.5453f);
    return random;
}

float4 main(VSOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float4 color = gInputTexture.Sample(gSampler, uv);

    // Quantize UVs so neighboring pixels share the same random value as small dots.
    float2 noiseCoord = floor(uv * max(gNoiseDensity, 1.0f));
    float timeSeed = floor(gTime * max(gSeedChangeRate, 0.0f));
    float random = rand2dTo1d(noiseCoord + float2(timeSeed, timeSeed * 1.37f));
    float noiseAmount = step(saturate(gNoiseThreshold), random) * saturate(gNoiseIntensity);

    color.rgb *= (1.0f - noiseAmount);
    return color;
}
