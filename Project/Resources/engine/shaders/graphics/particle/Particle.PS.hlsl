#include "Particle.hlsli"

struct Material {
   float4 color;            ///< 表示色のRGBA成分。
   float4x4 uvTransform;    ///< UV変換行列。
   float4 renderingParams;  ///< x: 輝度、y: アルファ閾値、z: トゥーン階調数、w: 煙の輪郭フェード幅。
   float4 effectParams;     ///< x: ソフト有効、y: 距離、z: 歪みピクセル、w: 歪み混合率。
   float4 sceneParams;      ///< x: 幅、y: 高さ、z: near、w: far。
   float4 projectionParams; ///< x: 平行投影フラグ、y: 歪みにRGフローマップを使用、z: 煙の濃度。
};

struct PixelShaderOutput {
   float4 color : SV_TARGET0; ///< 表示色のRGBA成分。
};

ConstantBuffer<Material> gMaterial : register(b0);

cbuffer ParticleDrawParameters : register(b1) {
   uint gRefractionPass; ///< 背景を歪ませる屈折パスなら1、本体色を描くパスなら0。
};

Texture2D<float4> gTexture : register(t0);
Texture2D<float4> gSceneColor : register(t1);
Texture2D<float> gSceneDepth : register(t2);
SamplerState gSampler : register(s0);

float LinearizeDepth(float depth) {
   float nearClip = gMaterial.sceneParams.z;
   float farClip = gMaterial.sceneParams.w;

   if (gMaterial.projectionParams.x > 0.5f) {
      return lerp(nearClip, farClip, depth);
   }

   return nearClip * farClip / max(farClip - depth * (farClip - nearClip), 0.0001f);
}

float SmokeHash(float2 cell) {
   const float2 kHashDirection = float2(127.1f, 311.7f);
   const float kHashScale = 43758.5453f;
   return frac(sin(dot(cell, kHashDirection)) * kHashScale);
}

float SmokeNoise(float2 position) {
   float2 cell = floor(position);
   float2 fraction = frac(position);
   float2 blend = fraction * fraction * (3.0f - 2.0f * fraction);
   return lerp(lerp(SmokeHash(cell), SmokeHash(cell + float2(1.0f, 0.0f)), blend.x),
      lerp(SmokeHash(cell + float2(0.0f, 1.0f)), SmokeHash(cell + float2(1.0f, 1.0f)), blend.x), blend.y);
}

float SmokeBillow(float2 uv, float age, float seed) {
   // 粒子ごとに位相を変えた三つの渦を移流させ、同じ画像の黒い円が並ぶ見た目を崩す。
   const float2 kSeedOffset = float2(13.7f, 7.3f);
   const float2 kFlowPerLifetime = float2(-0.45f, 0.65f);
   float2 flow = seed * kSeedOffset + age * kFlowPerLifetime;
   return SmokeNoise(uv * 3.0f + flow) * 0.55f + SmokeNoise(uv * 7.0f + flow * 1.37f) * 0.3f +
          SmokeNoise(uv * 15.0f - flow * 0.65f) * 0.15f;
}

PixelShaderOutput main(VertexShaderOutput input) {
   PixelShaderOutput output;
   float4x4 particleUvTransform =
      float4x4(input.uvTransform0, input.uvTransform1, input.uvTransform2, input.uvTransform3);

   // UV変換 & テクスチャサンプリング
   float4 transformedUV = mul(float4(input.texCoord, 0.0f, 1.0f), particleUvTransform);
   transformedUV = mul(transformedUV, gMaterial.uvTransform);

   if (input.customData.x < 0.0f) {
      // 通常パーティクルのClampサンプラーを維持したまま、トレイルの長手方向だけを繰り返す。
      transformedUV.y = frac(transformedUV.y);
   }

   // Texture atlases bleed between frames through generated mipmaps, so particles sample the base mip.
   float4 textureColor = gTexture.SampleLevel(gSampler, transformedUV.xy, 0.0f);

   // α=0ならピクセル破棄
   if (textureColor.a <= gMaterial.renderingParams.y) {
      discard;
   }

   output.color = textureColor * input.color * gMaterial.color;

   if (gMaterial.renderingParams.w > 0.0f && input.customData.x >= 0.0f) {
      // 画像の硬い輪郭をUV空間で薄め、重なった煙にも板状の境界を残さない。
      const float kSmokeFadeInFraction = 0.08f;
      float radius = length(input.texCoord * 2.0f - 1.0f);
      float edgeFade = 1.0f - smoothstep(1.0f - gMaterial.renderingParams.w, 1.0f, radius);
      output.color.a *= edgeFade * smoothstep(0.0f, kSmokeFadeInFraction, input.customData.x);
   }

   if (gMaterial.projectionParams.z > 0.0f && input.customData.x >= 0.0f) {
      const float3 kLuminanceWeights = float3(0.2126f, 0.7152f, 0.0722f);
      const float kOpticalDepthScale = 3.0f;
      float billow = SmokeBillow(input.texCoord, input.customData.x, input.customData.y);
      float textureDensity = saturate((dot(textureColor.rgb, kLuminanceWeights) - 0.35f) / 0.65f);
      float density = smoothstep(0.22f, 0.78f, billow) * lerp(0.4f, 1.0f, textureDensity);

      // 光学的厚みから透過率を求める。重なる場所だけ濃くし、薄い縁や渦の隙間を残す。
      float opticalDepth = output.color.a * gMaterial.projectionParams.z * density * kOpticalDepthScale;
      output.color.a = 1.0f - exp(-opticalDepth);
      output.color.rgb *= lerp(0.65f, 1.2f, billow);
   }

   float2 screenUV = input.position.xy / gMaterial.sceneParams.xy;

   if (gMaterial.effectParams.x > 0.5f) {
      float sceneDepth = gSceneDepth.SampleLevel(gSampler, screenUV, 0.0f);
      float depthSeparation = LinearizeDepth(sceneDepth) - LinearizeDepth(input.position.z);
      output.color.a *= saturate(depthSeparation / max(gMaterial.effectParams.y, 0.0001f));
   }

   // カスタムストリーム z はカメラ近接フェードとして標準マテリアルで利用する。
   output.color.a *= input.customData.z;

   if (gRefractionPass != 0) {
      // 通常画像は中心から放射状に歪ませ、明示指定されたRGフローマップだけを方向として解釈する。
      // 色から用途を推測しないことで、同じテクスチャがフレームごとに異なる方向へ歪むのを防ぐ。
      float2 encodedDirection = textureColor.rg * 2.0f - 1.0f;
      float2 radialDirection = input.texCoord * 2.0f - 1.0f;
      float useEncodedDirection = gMaterial.projectionParams.y > 0.5f ? 1.0f : 0.0f;
      float2 distortionDirection = lerp(radialDirection, encodedDirection, useEncodedDirection);
      float directionLengthSquared = dot(distortionDirection, distortionDirection);
      distortionDirection =
         directionLengthSquared > 0.000001f ? distortionDirection * rsqrt(directionLengthSquared) : float2(0.0f, 0.0f);

      float visibility = saturate(output.color.a);
      float2 inverseSceneSize = rcp(max(gMaterial.sceneParams.xy, float2(1.0f, 1.0f)));
      float2 distortionOffset = distortionDirection * gMaterial.effectParams.z * visibility * inverseSceneSize;
      float2 halfTexel = inverseSceneSize * 0.5f;
      float2 distortedUV = clamp(screenUV + distortionOffset, halfTexel, 1.0f - halfTexel);
      float4 distortedScene = gSceneColor.SampleLevel(gSampler, distortedUV, 0.0f);
      float distortionWeight = saturate(visibility * gMaterial.effectParams.w);

      // 背景コピーはシステム内で共有されるため、元背景と混ぜて不透明出力すると
      // 透明な粒子まで先に描いた粒子・リボンの歪みを消してしまう。
      // 現在の描画先との合成はNormalブレンドに任せ、フェードと混合率をalphaに保持する。
      // 粒子本体の色・輝度は別パスで元のブレンドを使い、屈折の有無や混合率から独立させる。
      output.color = float4(distortedScene.rgb, distortionWeight);
   } else {
      float toonSteps = gMaterial.renderingParams.z;

      if (toonSteps >= 2.0f) {
         output.color.rgb = floor(saturate(output.color.rgb) * (toonSteps - 1.0f) + 0.5f) / (toonSteps - 1.0f);
      }

      output.color.rgb *= gMaterial.renderingParams.x;
   }

   if (output.color.a <= gMaterial.renderingParams.y) {
      discard;
   }

   return output;
}
