#include "Trail.hlsli"

cbuffer TrailTransform : register(b0) {
   row_major float4x4 gViewProjection; ///< ワールド位置をクリップ空間へ変換する行列。
};

struct VertexShaderInput {
   float3 position : POSITION0; ///< 記録済みの世界座標（m）。
   float4 color : COLOR0;       ///< 寿命による減衰を適用したRGBA。
   float side : TEXCOORD0;      ///< 帯の幅方向の座標（左端-1、右端1）。
};

TrailInterpolants main(VertexShaderInput input) {
   TrailInterpolants output;
   output.position = mul(float4(input.position, 1.0f), gViewProjection);
   output.color = input.color;
   output.side = input.side;
   return output;
}
