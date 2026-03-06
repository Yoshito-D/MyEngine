#include "FullscreenTriangle.hlsli"

Texture2D gInputTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VSOutput input) : SV_TARGET
{
    // 元の色を取得
    float4 color = gInputTexture.Sample(gSampler, input.texcoord);

    // 輝度計算（NTSC係数）
    float gray = dot(color.rgb, float32_t3(0.299, 0.587, 0.114));

    // グレースケール化
    return float32_t4(gray, gray, gray, color.a);
}
