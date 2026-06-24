#include "FullscreenTriangle.hlsli"

cbuffer BloomParams : register(b0)
{
    float threshold;        // 輝度閾値
    float intensity;        // ブルーム強度
    float blurRadius;       // ブラー半径
    float softThreshold;    // ソフト閾値（閾値付近の滑らかさ）
};

Texture2D inputTexture : register(t0);
SamplerState samplerLinear : register(s0);

// 輝度計算
float Luminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

// ソフト閾値を使用したブライトパス計算
float SoftThreshold(float luminance, float threshold, float softness)
{
    // ソフトニーカーブ（smoothstepの拡張版）
    float knee = threshold * softness;
    float soft = luminance - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.00001);
    
    float contribution = max(soft, luminance - threshold);
    return contribution / max(luminance, 0.00001);
}

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    float4 color = inputTexture.Sample(samplerLinear, uv);
    
    // テクスチャサイズを取得
    float2 texelSize;
    inputTexture.GetDimensions(texelSize.x, texelSize.y);
    texelSize = 1.0 / texelSize * blurRadius;
    
    // ブライトパス抽出とブラーを同時に適用
    float3 bloomColor = float3(0, 0, 0);
    float weightSum = 0.0;
    
    // 9タップガウシアンブラー
    const int kernelRadius = 2;
    const float sigma = 1.5;
    
    for (int x = -kernelRadius; x <= kernelRadius; ++x)
    {
        for (int y = -kernelRadius; y <= kernelRadius; ++y)
        {
            float2 offset = float2(x, y) * texelSize;
            float3 sample = inputTexture.Sample(samplerLinear, uv + offset).rgb;
            
            // 輝度計算
            float lum = Luminance(sample);
            
            // ソフト閾値を使用したブライトパス抽出
            float brightFactor = SoftThreshold(lum, threshold, softThreshold);
            float3 brightSample = sample * brightFactor;
            
            // ガウス重み計算
            float distance = length(float2(x, y));
            float weight = exp(-(distance * distance) / (2.0 * sigma * sigma));
            
            bloomColor += brightSample * weight;
            weightSum += weight;
        }
    }
    
    bloomColor /= max(weightSum, 0.001);
    
    // 元の色にブルームを加算合成
    float3 finalColor = color.rgb + bloomColor * intensity;
    
    return float4(finalColor, color.a);
}
