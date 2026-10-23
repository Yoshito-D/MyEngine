#include "FullscreenTriangle.hlsli"

Texture2D inputTexture : register(t0);
SamplerState samplerLinear : register(s0);

// ブラーパラメータ用の定数バッファ
cbuffer FilterParams : register(b0) {
   float intensity; // ブラー強度
   int kernelSize;  // カーネルサイズ
   float sigma;     // ガウシアンのシグマ値
};

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET {
   // 画面サイズを動的に取得
   uint width, height;
   inputTexture.GetDimensions(width, height);
   float2 texelSize = float2(1.0f / width, 1.0f / height);

   // カーネルサイズを適用
   texelSize *= kernelSize;

   float4 color = float4(0, 0, 0, 0);
   float totalWeight = 0.0f;

   // 最低でも3×3の範囲をサンプリングするため、半径を1以上にする。
   int kernelRadius = max(1, kernelSize);

   // 中心からの距離に応じて周辺画素を重み付けし、2次元のぼかし色を求める。
   for (int x = -kernelRadius; x <= kernelRadius; x++) {
      for (int y = -kernelRadius; y <= kernelRadius; y++) {
         float2 offset = float2(x, y) * texelSize;

         // sigmaが大きいほど離れたサンプルにも重みを残し、ぼかしを広げる。
         float distance = length(float2(x, y));
         float weight = exp(-distance * distance / (2.0f * sigma * sigma));

         color += inputTexture.Sample(samplerLinear, uv + offset) * weight;
         totalWeight += weight;
      }
   }

   // サンプル数やsigmaによって画像全体の明るさが変わらないよう、重みの総和で割る。
   color /= totalWeight;

   // intensityで元画像とぼかし画像の混合比率を指定する。
   float4 originalColor = inputTexture.Sample(samplerLinear, uv);
   return lerp(originalColor, color, intensity);
}
