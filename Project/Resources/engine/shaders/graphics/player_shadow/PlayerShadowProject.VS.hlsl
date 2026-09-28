#include "PlayerShadow.hlsli"

ConstantBuffer<PlayerShadowProjectConstants> gProjectConstants : register(b0);

/// @brief 惑星の画面座標と影の投影座標を生成する（未実装）。
PlayerShadowProjectVertexOutput main(PlayerShadowVertexInput input) {
    PlayerShadowProjectVertexOutput output;

    // 深度EQUALで重ねるため、通常描画と同じWVP・同じ計算式にする。
    output.position =
        mul(input.position, gProjectConstants.worldViewProjection);

    float4 worldPosition =
        mul(input.position, gProjectConstants.world);

    output.worldPosition = worldPosition.xyz;

    output.worldNormal = normalize(
        mul(input.normal,
            (float3x3) gProjectConstants.worldInverseTranspose)
    );

    // この地面の位置が、影用カメラからどこに見えるかを求める。
    output.shadowPosition =
        mul(worldPosition, gProjectConstants.shadowViewProjection);

    return output;
}
