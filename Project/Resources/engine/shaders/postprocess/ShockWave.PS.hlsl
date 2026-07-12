#include "FullscreenTriangle.hlsli"

cbuffer ShockWaveCB : register(b0)
{
    float32_t2 gCenter;           // ショックウェーブの中心
    float32_t2 gAspectRatio;      // アスペクト比補正
    float gWaveRadius;            // 波の半径
    float gWaveThickness;         // 波の厚さ
    float gDistortionStrength;    // 歪みの強さ
    float gFadeOutRadius;         // フェードアウト開始半径
    float gHighlightIntensity;    // ハイライト強度
};

Texture2D gInputTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VSOutput input) : SV_TARGET
{
    float32_t2 uv = input.texcoord;
    
    // アスペクト比を補正したUV座標を計算
    float32_t2 correctedUV = (uv - gCenter) * gAspectRatio + gCenter;
    float32_t2 centerOffset = correctedUV - gCenter;
    float distance = length(centerOffset);
    
    // ショックウェーブの強度を計算（waveRadiusを使用）
    float waveFront = abs(distance - gWaveRadius);
    float waveIntensity = 1.0 - smoothstep(0.0, gWaveThickness, waveFront);
    
    // フェードアウト効果
    float fadeOut = 1.0 - smoothstep(gFadeOutRadius * 0.5, gFadeOutRadius, distance);
    waveIntensity *= fadeOut;
    
    // 歪みベクトルを計算（元のUV座標に対して適用）
    float32_t2 distortionVector = float32_t2(0, 0);
    if (distance > 0.001)
    {
        // 中心から外向きの歪み（アスペクト比補正を元に戻す）
        float32_t2 normalizedOffset = normalize(centerOffset);
        distortionVector = normalizedOffset * gDistortionStrength * waveIntensity / gAspectRatio;
    }
    
    // 歪んだUV座標でテクスチャをサンプル
    float32_t2 distortedUV = uv + distortionVector;
    
    // 画面境界をクランプ
    distortedUV = clamp(distortedUV, 0.0, 1.0);
    
    float32_t4 color = gInputTexture.Sample(gSampler, distortedUV);
    
    // ショックウェーブの輪郭にハイライト効果を追加（可変強度）
    float highlightIntensity = waveIntensity * gHighlightIntensity;
    color.rgb += highlightIntensity;
    
    return color;
}