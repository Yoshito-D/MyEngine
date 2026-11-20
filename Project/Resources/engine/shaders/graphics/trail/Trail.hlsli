struct TrailInterpolants {
   float4 position : SV_POSITION; ///< クリップ空間の頂点座標。
   float4 color : COLOR0;         ///< 寿命による減衰を適用したRGBA。
   float side : TEXCOORD0;        ///< 帯の幅方向の座標（左端-1、右端1）。
};
