#include "PlayerShadow.hlsli"

ConstantBuffer<PlayerShadowParameters> gShadowParameters : register(b1);
Texture2D<float4> gShadowMask : register(t0);
SamplerState gShadowSampler : register(s0);

/// @brief 惑星に投影されたシルエットを黒のα合成として出力する（未実装）。
float4 main(PlayerShadowProjectVertexOutput input) : SV_TARGET0 {
    if (input.shadowPosition.w <= 0.0f)
    {
        discard;
    }

    float3 ndc = input.shadowPosition.xyz / input.shadowPosition.w;

    // DirectXのクリップ範囲：XYは[-1,1]、Zは[0,1]。
    if (any(abs(ndc.xy) > 1.0f) ||
        ndc.z < 0.0f || ndc.z > 1.0f)
    {
        discard;
    }

    float3 shadowUp = gShadowParameters.shadowUpAndOpacity.xyz;
    float opacity = gShadowParameters.shadowUpAndOpacity.w;

    float3 normal = normalize(input.worldNormal);

    // 惑星の反対側にも同じマスクが投影されるのを防ぐ。
    if (dot(normal, shadowUp) <= 0.0f)
    {
        discard;
    }

    float3 groundPosition =
        gShadowParameters.groundPositionAndRange.xyz;
    float receiverRange =
        gShadowParameters.groundPositionAndRange.w;

    // 真下の地表付近だけに投影を許可する。
    float3 offset = input.worldPosition - groundPosition;
    if (dot(offset, offset) > receiverRange * receiverRange)
    {
        discard;
    }

    // クリップ座標からテクスチャ座標へ変換する。
    // Y反転はここで一度だけ行う。
    float2 uv = ndc.xy * float2(0.5f, -0.5f) + 0.5f;

    // マスクは1ミップなので、レベル0を明示して読む。
    float mask = gShadowMask.SampleLevel(gShadowSampler, uv, 0.0f).r;

    return float4(0.0f, 0.0f, 0.0f, saturate(mask * opacity));
}
