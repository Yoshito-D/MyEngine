#include "FullscreenTriangle.hlsli"

cbuffer DissolveCB : register(b0)
{
    float gThreshold;
    float gEdgeWidth;
    float gEdgeIntensity;
    float gMaskContrast;
    float2 gMaskTiling;
    float2 gMaskOffset;
    float4 gEdgeColor;
    float4 gDissolveColor;
};

Texture2D gInputTexture : register(t0);
Texture2D gMaskTexture : register(t1);
SamplerState gInputSampler : register(s0);
SamplerState gMaskSampler : register(s1);

float4 main(VSOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float4 baseColor = gInputTexture.Sample(gInputSampler, uv);

    float2 maskUV = uv * max(gMaskTiling, float2(0.0001f, 0.0001f)) + gMaskOffset;
    float maskValue = gMaskTexture.Sample(gMaskSampler, maskUV).r;
    maskValue = saturate((maskValue - 0.5f) * gMaskContrast + 0.5f);

    float width = max(gEdgeWidth, 0.0001f);
    float visible = smoothstep(gThreshold - width, gThreshold + width, maskValue);
    float edgeMask = saturate(1.0f - abs(maskValue - gThreshold) / width);
    visible = (gThreshold <= 0.0f) ? 1.0f : visible;
    visible = (gThreshold >= 1.0f) ? 0.0f : visible;
    edgeMask *= smoothstep(0.0f, 1.0f, gThreshold);
    edgeMask *= max(gEdgeIntensity, 0.0f) * gEdgeColor.a;

    float3 color = lerp(gDissolveColor.rgb, baseColor.rgb, visible);
    color = lerp(color, gEdgeColor.rgb, saturate(edgeMask));

    float alpha = lerp(gDissolveColor.a, baseColor.a, visible);
    return float4(saturate(color), alpha);
}
