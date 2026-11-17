#include "FullscreenTriangle.hlsli"

// Bloom::BloomCBのパス番号・重み配列と一致させる。
static const uint kCopyPass = 0;
static const uint kHorizontalPass = 1;
static const uint kWeightVectorCount = 12;
static const uint kWeightsPerVector = 4;

cbuffer BloomParams : register(b0) {
   float threshold;     ///< 発光を抽出する輝度しきい値。
   float intensity;     ///< 周辺へ加算する発光の強度。
   float softThreshold; ///< しきい値の境界の柔らかさ。
   int kernelRadius;    ///< 片側のサンプル数（ピクセル）。
   uint pass;           ///< 元画像のコピー・横ぼかし・縦ぼかし合成の選択。
   float3 padding;
   float4 weights[kWeightVectorCount]; ///< 対称ガウシアンの中心から片側の正規化済み重み。
};

Texture2D<float4> inputTexture : register(t0);
Texture2D<float4> blurTexture : register(t1);
SamplerState samplerLinear : register(s0);

float GaussianWeight(uint offset) {
   return weights[offset / kWeightsPerVector][offset % kWeightsPerVector];
}

float3 ExtractBrightColor(float2 uv) {
   const float3 kLuminanceWeights = float3(0.2126f, 0.7152f, 0.0722f);
   const float kLuminanceEpsilon = 0.00001f;
   float3 color = max(inputTexture.SampleLevel(samplerLinear, uv, 0.0f).rgb, 0.0f);
   float luminance = dot(color, kLuminanceWeights);

   // ソフトニーでしきい値付近の光だけを滑らかに立ち上げ、硬い抽出境界を防ぐ。
   float knee = threshold * softThreshold;
   float soft = clamp(luminance - threshold + knee, 0.0f, 2.0f * knee);
   soft = soft * soft / max(4.0f * knee, kLuminanceEpsilon);
   float contribution = max(soft, luminance - threshold);
   return color * (contribution / max(luminance, kLuminanceEpsilon));
}

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET {
   float4 color = inputTexture.SampleLevel(samplerLinear, uv, 0.0f);

   if (pass == kCopyPass) {
      return color;
   }

   uint width, height;
   inputTexture.GetDimensions(width, height);
   float2 texelSize = 1.0f / float2(width, height);
   float3 bloomColor = float3(0.0f, 0.0f, 0.0f);

   if (pass == kHorizontalPass) {
      // 原寸の全画素で発光を抽出してから混ぜる。細いリムライトもサンプル間へ落とさない。
      [loop] for (int offset = -kernelRadius; offset <= kernelRadius; ++offset) {
         bloomColor += ExtractBrightColor(uv + float2(offset * texelSize.x, 0.0f)) * GaussianWeight(abs(offset));
      }

      return float4(bloomColor, 0.0f);
   }

   // 抽出済みの中間画像では隣接2画素を線形補間でまとめて読み、同じガウシアンを少ない取得回数で評価する。
   bloomColor = blurTexture.SampleLevel(samplerLinear, uv, 0.0f).rgb * GaussianWeight(0);

   [loop] for (int offset = 1; offset <= kernelRadius; offset += 2) {
      float firstWeight = GaussianWeight(offset);
      float secondWeight = offset < kernelRadius ? GaussianWeight(offset + 1) : 0.0f;
      float combinedWeight = firstWeight + secondWeight;

      // ごく小さな半径で重みがアンダーフローした場合も、補間位置を0除算で壊さない。
      if (combinedWeight == 0.0f) {
         continue;
      }

      float sampleOffset = float(offset) + secondWeight / combinedWeight;
      float2 delta = float2(0.0f, sampleOffset * texelSize.y);
      bloomColor += blurTexture.SampleLevel(samplerLinear, uv + delta, 0.0f).rgb * combinedWeight;
      bloomColor += blurTexture.SampleLevel(samplerLinear, uv - delta, 0.0f).rgb * combinedWeight;
   }

   // 元画像の細部とアルファを保持し、HDRの発光だけを縦横同じ広がりで加算する。
   return float4(color.rgb + bloomColor * intensity, color.a);
}
