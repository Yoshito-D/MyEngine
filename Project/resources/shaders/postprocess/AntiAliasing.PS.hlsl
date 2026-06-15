#include "FullscreenTriangle.hlsli"

struct AntiAliasingParams
{
    float contrastThreshold;
    float relativeThreshold;
    float subpixelBlending;
    float edgeSearchSteps;
};

ConstantBuffer<AntiAliasingParams> gParams : register(b0);
Texture2D<float4> gInputTexture : register(t0);
SamplerState gSampler : register(s0);

static const float3 kLuma = float3(0.299f, 0.587f, 0.114f);

float Luminance(float3 color)
{
    return dot(color, kLuma);
}

float3 SampleColor(float2 uv)
{
    return gInputTexture.SampleLevel(gSampler, saturate(uv), 0.0f).rgb;
}

float4 main(VSOutput input) : SV_TARGET
{
    uint width;
    uint height;
    gInputTexture.GetDimensions(width, height);
    float2 texelSize = float2(rcp((float)width), rcp((float)height));

    float4 sampleM = gInputTexture.SampleLevel(gSampler, input.texcoord, 0.0f);
    float3 colorM = sampleM.rgb;
    float3 colorNW = SampleColor(input.texcoord + texelSize * float2(-1.0f, -1.0f));
    float3 colorNE = SampleColor(input.texcoord + texelSize * float2(1.0f, -1.0f));
    float3 colorSW = SampleColor(input.texcoord + texelSize * float2(-1.0f, 1.0f));
    float3 colorSE = SampleColor(input.texcoord + texelSize * float2(1.0f, 1.0f));

    float lumaM = Luminance(colorM);
    float lumaNW = Luminance(colorNW);
    float lumaNE = Luminance(colorNE);
    float lumaSW = Luminance(colorSW);
    float lumaSE = Luminance(colorSE);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;
    float edgeThreshold = max(gParams.contrastThreshold, lumaMax * gParams.relativeThreshold);
    if (lumaRange < edgeThreshold)
    {
        return sampleM;
    }

    float2 direction;
    direction.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    direction.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    static const float kReduceMul = 1.0f / 8.0f;
    static const float kReduceMin = 1.0f / 128.0f;
    float directionReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25f * kReduceMul), kReduceMin);
    float inverseDirectionAdjustment = rcp(min(abs(direction.x), abs(direction.y)) + directionReduce);

    float maxSearch = max(gParams.edgeSearchSteps, 1.0f);
    direction = clamp(direction * inverseDirectionAdjustment, -maxSearch, maxSearch) * texelSize;

    float3 colorA =
        0.5f * (
            SampleColor(input.texcoord + direction * (1.0f / 3.0f - 0.5f)) +
            SampleColor(input.texcoord + direction * (2.0f / 3.0f - 0.5f)));

    float3 colorB =
        colorA * 0.5f +
        0.25f * (
            SampleColor(input.texcoord + direction * -0.5f) +
            SampleColor(input.texcoord + direction * 0.5f));

    float lumaB = Luminance(colorB);
    float3 fxaaColor = (lumaB < lumaMin || lumaB > lumaMax) ? colorA : colorB;

    float subpixelAmount = saturate(gParams.subpixelBlending);
    float3 result = lerp(colorM, fxaaColor, subpixelAmount);
    return float4(result, sampleM.a);
}
