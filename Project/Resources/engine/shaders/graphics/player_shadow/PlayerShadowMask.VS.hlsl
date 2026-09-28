#include "PlayerShadow.hlsli"

ConstantBuffer<PlayerShadowMaskConstants> gMaskConstants : register(b0);

/// @brief プレイヤー頂点を影用の正射影座標へ変換する（未実装）。
PlayerShadowMaskVertexOutput main(PlayerShadowVertexInput input) {
    PlayerShadowMaskVertexOutput output;

    // ローカル座標 → ワールド座標 → 影用カメラのクリップ座標。
    // CPU側ですでに合成された行列を使う。
    output.position = mul(input.position, gMaskConstants.playerShadowWVP);

    output.texCoord = input.texCoord;
    return output;
}
