#include "Trail.hlsli"

cbuffer TrailDepthProjection : register(b1) {
   float4 gDepthProjection; ///< 射影行列の m[2][2]、m[2][3]、m[3][2]、m[3][3]。
};

Texture2D<float> gSceneDepth : register(t0);

float ToViewDepth(float depth) {
   // z/wからビュー空間のz（m）を逆算する。透視投影と平行投影で同じ式を使う。
   return (depth * gDepthProjection.w - gDepthProjection.z) /
          max(gDepthProjection.x - depth * gDepthProjection.y, 0.000001f);
}

float ContactVisibility(float sceneDepth, float trailViewDepth) {
   const float kContactFadeDistance = 0.08f; ///< 不透明面と交差する箇所をぼかす深度差（m）。
   return smoothstep(0.0f, kContactFadeDistance, ToViewDepth(sceneDepth) - trailViewDepth);
}

float ReadDepth(int2 pixel, int2 lastPixel) {
   return gSceneDepth.Load(int3(clamp(pixel, int2(0, 0), lastPixel), 0));
}

float4 main(TrailInterpolants input) : SV_TARGET0 {
   // 帯の中心から縁へ向かって透明にし、板状の輪郭を残さない。
   float4 color = input.color;
   color.a *= 1.0f - smoothstep(0.0f, 1.0f, abs(input.side));

   uint width, height;
   gSceneDepth.GetDimensions(width, height);
   int2 lastPixel = int2(width, height) - 1;
   int2 pixel = int2(input.position.xy);
   float trailViewDepth = ToViewDepth(input.position.z);
   float visibility = ContactVisibility(ReadDepth(pixel, lastPixel), trailViewDepth);

   // シルエットの外側では深度が急に遠くなるため、隣接画素も見て輪郭の1画素をなじませる。
   float nearbyDepth = min(min(ReadDepth(pixel + int2(-1, 0), lastPixel), ReadDepth(pixel + int2(1, 0), lastPixel)),
      min(ReadDepth(pixel + int2(0, -1), lastPixel), ReadDepth(pixel + int2(0, 1), lastPixel)));
   const float kSilhouetteMinVisibility = 0.65f;
   color.a *= visibility * lerp(kSilhouetteMinVisibility, 1.0f, ContactVisibility(nearbyDepth, trailViewDepth));
   return color;
}
