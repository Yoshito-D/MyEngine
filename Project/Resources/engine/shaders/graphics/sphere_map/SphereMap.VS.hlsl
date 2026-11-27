#include "SphereMap.hlsli"

cbuffer MapItem : register(b0) {
   float4 placement; // XYはクリップ空間中心、ZWは描画する矩形の半幅・半高。
   float4 rays;      // 矩形の中心と端に向かう視線の傾き。
   float4 sphere;
   float4 tint;
   float4 axisX;
   float4 axisY;
   float4 aperture;
};

MapVertex main(uint index : SV_VertexID) {
   static const float2 kCorners[] = {
      float2(-1.0f, -1.0f), float2(1.0f, -1.0f), float2(-1.0f, 1.0f),
      float2(-1.0f, 1.0f), float2(1.0f, -1.0f), float2(1.0f, 1.0f)
   };
   MapVertex output;
   const float2 corner = kCorners[index];
   output.local = sphere.w > 0.0f ? rays.xy + rays.zw * corner : corner;
   output.position = float4(placement.xy + placement.zw * corner * float2(1.0f, -1.0f), 0.0f, 1.0f);
   output.sphere = sphere;
   output.tint = tint;
   output.axisX = axisX;
   output.axisY = axisY;
   output.aperture = aperture;
   return output;
}
