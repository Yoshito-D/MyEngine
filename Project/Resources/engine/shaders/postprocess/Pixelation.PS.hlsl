#include "FullscreenTriangle.hlsli"

struct PixelationParams {
   float pixelSize;   ///< ピクセル化で同じ色にまとめるブロックの辺長。
   float2 screenSize; ///< 画面の幅と高さ（ピクセル）。
   float padding;     ///< GPU転送データの配置境界を揃える未使用領域。
};

Texture2D<float4> sourceTexture : register(t0);
SamplerState textureSampler : register(s0);
ConstantBuffer<PixelationParams> params : register(b0);

struct PSOutput {
   float4 color : SV_TARGET0; ///< 表示色のRGBA成分。
};

PSOutput main(VSOutput input) {
   PSOutput output;

   // 現在のピクセル位置を取得
   float2 screenPos = input.texcoord * params.screenSize;

   // ピクセルサイズに基づいてグリッドに合わせる
   float2 gridPos = floor(screenPos / params.pixelSize) * params.pixelSize;

   // グリッドの中心にオフセット
   gridPos += params.pixelSize * 0.5;

   // 正規化されたテクスチャ座標に戻す
   float2 pixelatedUV = gridPos / params.screenSize;

   // 範囲チェック
   if (pixelatedUV.x < 0.0 || pixelatedUV.x > 1.0 || pixelatedUV.y < 0.0 || pixelatedUV.y > 1.0) {
      output.color = float4(0.0, 0.0, 0.0, 1.0);
      return output;
   }

   // ピクセル化されたテクスチャからサンプリング
   output.color = sourceTexture.Sample(textureSampler, pixelatedUV);

   return output;
}
