#include "PlayerShadow.hlsli"

/// @brief マスクへモデルのシルエットを白で出力する。
float4 main(PlayerShadowMaskVertexOutput input) : SV_TARGET0 {
    return float4(1.0f, 1.0f, 1.0f, 1.0f);

}
