struct MapVertex {
   float4 position : SV_Position;
   float2 local : TEXCOORD0;                       // 球は視線の傾き、光点は正方形内の座標。
   nointerpolation float4 sphere : TEXCOORD1;      // カメラ空間の中心と半径（m）。半径0は光点、負は円周。
   nointerpolation float4 tint : TEXCOORD2;        // 線形RGBA色。
   nointerpolation float4 axisX : TEXCOORD3;       // XYZは惑星のX軸、Wは線または光点の強調度。
   nointerpolation float4 axisY : TEXCOORD4;       // XYZは惑星のY軸、WはUIピクセルあたりのサンプル数。
   nointerpolation float4 aperture : TEXCOORD5;   // XYは円の中心、Zは半径（UIピクセル）、Wは近クリップ距離（m）。
};
