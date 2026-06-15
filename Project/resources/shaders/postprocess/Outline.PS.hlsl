#include "FullscreenTriangle.hlsli"

struct OutlineParams
{
    float4 outlineColor;
    float2 texelSize;
    float thickness;
    float depthThreshold;
    float intensity;
};

ConstantBuffer<OutlineParams> gParams : register(b0);
Texture2D<float4> gInputTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);
SamplerState gColorSampler : register(s0);
SamplerState gDepthSampler : register(s1);

static const float32_t kPrewittHorizontalKernel[3][3] =
{
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f }
};

static const float32_t kPrewittVerticalKernel[3][3] =
{
    { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
    { 0.0f, 0.0f, 0.0f },
    { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f }
};

static const float32_t2 kIndex3x3[3][3] =
{
    { float32_t2(-1, -1), float32_t2(0, -1), float32_t2(1, -1) },
    { float32_t2(-1, 0), float32_t2(0, 0), float32_t2(1, 0) },
    { float32_t2(-1, 1), float32_t2(0, 1), float32_t2(1, 1) }
};

float ConvertNdcDepthToViewZ(float ndcDepth)
{
    static const float kNearClip = 0.01f;
    static const float kFarClip = 10000.0f;
    return (kNearClip * kFarClip) / max(kFarClip - ndcDepth * (kFarClip - kNearClip), 0.000001f);
}

float SampleViewZ(float2 uv)
{
    float ndcDepth = gDepthTexture.SampleLevel(gDepthSampler, saturate(uv), 0.0f);
    return ConvertNdcDepthToViewZ(ndcDepth);
}

float4 main(VSOutput input) : SV_TARGET
{
    float2 difference = float2(0.0f, 0.0f);
    float2 sampleStep = gParams.texelSize * max(gParams.thickness, 0.0f);
    
    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            float2 texcoord = input.texcoord + kIndex3x3[x][y] * sampleStep;
            float viewZ = SampleViewZ(texcoord);
            
            difference.x += viewZ * kPrewittHorizontalKernel[x][y];
            difference.y += viewZ * kPrewittVerticalKernel[x][y];
        }
    }
    
    float edge = length(difference);
    float edgeWeight = saturate(edge / max(gParams.depthThreshold, 0.000001f));
    edgeWeight *= saturate(gParams.intensity) * saturate(gParams.outlineColor.a);

    float4 baseColor = gInputTexture.Sample(gColorSampler, input.texcoord);
    float3 outlinedColor = lerp(baseColor.rgb, gParams.outlineColor.rgb, edgeWeight);
    return float4(outlinedColor, baseColor.a);
}
