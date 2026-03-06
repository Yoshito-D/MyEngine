#include "FullscreenTriangle.hlsli"

Texture2D inputTexture : register(t0);
SamplerState samplerLinear : register(s0);

// ブラーパラメータ用の定数バッファ（知り合いのコードに合わせて）
cbuffer BlurParams : register(b0)
{
    float intensity;      // ブラー強度
    float kernelSize;     // カーネルサイズ
    float sigma;          // ガウシアンのシグマ値
    float padding;        // パディング
};

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    // 画面サイズを動的に取得
    uint width, height;
    inputTexture.GetDimensions(width, height);
    float2 texelSize = float2(1.0f / width, 1.0f / height);
    
    // カーネルサイズを適用
    texelSize *= kernelSize;

    float4 color = float4(0, 0, 0, 0);
    float totalWeight = 0.0f;

    // 可変サイズのブラーカーネル（知り合いのコードと同様）
    int kernelRadius = max(1, (int)(kernelSize + 0.5f)); // 四捨五入

    // 2Dガウシアンブラー（知り合いのコードと同じ構造）
    for (int x = -kernelRadius; x <= kernelRadius; x++)
    {
        for (int y = -kernelRadius; y <= kernelRadius; y++)
        {
            float2 offset = float2(x, y) * texelSize;

            // ガウシアン重みの計算（知り合いのコードと同じ方式）
            float distance = length(float2(x, y));
            float weight = exp(-distance * distance / (2.0f * sigma * sigma));

            color += inputTexture.Sample(samplerLinear, uv + offset) * weight;
            totalWeight += weight;
        }
    }

    // 重みで正規化（知り合いのコードと同様）
    color /= totalWeight;

    // 強度を適用（元の色とブラー色をブレンド - 知り合いのコードと同様）
    float4 originalColor = inputTexture.Sample(samplerLinear, uv);
    return lerp(originalColor, color, intensity);
}
