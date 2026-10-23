#ifndef PLAYER_SHADOW_HLSLI
#define PLAYER_SHADOW_HLSLI

struct PlayerShadowMaskConstants {
   row_major float4x4 playerShadowWVP; ///< プレイヤー頂点を影マスクのクリップ空間へ移す行列。
};

struct PlayerShadowProjectConstants {
   row_major float4x4 worldViewProjection;   ///< ローカル頂点を画面のクリップ空間へ移す合成行列。
   row_major float4x4 world;                 ///< ローカル頂点をワールド座標系へ移す変換行列。
   row_major float4x4 worldInverseTranspose; ///< ワールド行列の逆行列の転置行列。
   row_major float4x4 shadowViewProjection;  ///< ワールド位置を影マスクの投影空間へ移す行列。
};

struct PlayerShadowParameters {
   float4 shadowUpAndOpacity;     ///< xyz: 惑星の外向き、w: 濃さ。
   float4 groundPositionAndRange; ///< xyz: 投影中心、w: 受け面の許可範囲。
};

struct PlayerShadowVertexInput {
   float4 position : POSITION0; ///< 描画する頂点のローカル位置と同次座標成分。
   float2 texCoord : TEXCOORD0; ///< テクスチャ座標。
   float3 normal : NORMAL0;     ///< 面の向きを表す法線ベクトル。
};

struct PlayerShadowMaskVertexOutput {
   float4 position : SV_POSITION; ///< ラスタライズに使うクリップ空間の同次位置。
   float2 texCoord : TEXCOORD0;   ///< テクスチャ座標。
};

struct PlayerShadowProjectVertexOutput {
   float4 position : SV_POSITION;     ///< ラスタライズに使うクリップ空間の同次位置。
   float4 shadowPosition : TEXCOORD0; ///< 影マスクの投影空間で表した同次位置。
   float3 worldPosition : TEXCOORD1;  ///< カメラのワールド位置。
   float3 worldNormal : TEXCOORD2;    ///< ワールド座標系で表した面の法線方向。
};

#endif
