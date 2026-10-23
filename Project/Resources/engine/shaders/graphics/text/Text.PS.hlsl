struct PixelShaderInput {
   float4 position : SV_POSITION;      ///< ラスタライズ後の画面位置と深度。
   float2 texCoord : TEXCOORD0;        ///< テクスチャ座標。
   float4 color : COLOR0;              ///< 表示色のRGBA成分。
   float4 atlasParameters : TEXCOORD1; ///< xyはアトラス寸法、zは距離範囲、wはMSDFなどのアトラス形式。
};

Texture2D<float4> gFontAtlas : register(t0);
SamplerState gFontSampler : register(s0);

float Median(float red, float green, float blue) {
   return max(min(red, green), min(max(red, green), blue));
}

float4 main(PixelShaderInput input) : SV_TARGET0 {
   const float4 atlasSample = gFontAtlas.Sample(gFontSampler, input.texCoord);
   float coverage = atlasSample.r;

   if (input.atlasParameters.w >= 1.0f) {
      const float signedDistance = Median(atlasSample.r, atlasSample.g, atlasSample.b);
      const float2 atlasSize = max(input.atlasParameters.xy, float2(1.0f, 1.0f));
      const float2 unitRange = input.atlasParameters.z / atlasSize;
      const float2 uvDelta = max(fwidth(input.texCoord), float2(1.0e-6f, 1.0e-6f));
      const float2 screenTextureSize = 1.0f / uvDelta;
      const float screenPixelRange = max(0.5f * dot(unitRange, screenTextureSize), 1.0f);
      coverage = saturate(screenPixelRange * (signedDistance - 0.5f) + 0.5f);
   }

   float4 outputColor = input.color;
   outputColor.a *= coverage;

   if (outputColor.a <= 0.001f) {
      discard;
   }

   return outputColor;
}
