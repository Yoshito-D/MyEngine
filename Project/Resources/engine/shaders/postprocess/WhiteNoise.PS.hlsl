#include "FullscreenTriangle.hlsli"

// C++側のWhiteNoiseCBと同じ順序。timeは描画時に進み、density／rate／threshold／intensityは
// CPU側でも有効範囲へ正規化されるが、シェーダー側にも安全な下限・飽和を残す。
cbuffer WhiteNoiseCB : register(b0)
{
    float gTime;
    float gNoiseDensity;
    float gSeedChangeRate;
    float gNoiseThreshold;
    float gNoiseIntensity;
};

Texture2D gInputTexture : register(t0);
SamplerState gSampler : register(s0);

// 2次元セル座標を安価で決定的な0～1の疑似乱数へ写すハッシュ。
// 非整数の内積係数でX/Yの規則性を崩し、sinと大きな乗数の小数部だけを使って近傍セルを非相関に見せる。
// 暗号用途の乱数ではなく、同じ座標と時間シードから毎回同じ模様を再現する描画用関数である。
float rand2dTo1d(float2 value, float2 dotDir = float2(12.9898f, 78.233f))
{
    float2 smallValue = sin(value);
    float random = dot(smallValue, dotDir);
    random = frac(sin(random) * 143758.5453f);
    return random;
}

float4 main(VSOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float4 color = gInputTexture.Sample(gSampler, uv);

    // UVをdensity個の格子へ量子化し、同じセル内の全ピクセルで乱数を共有する。
    // 下限1によりゼロ密度でも座標計算を成立させる。
    float2 noiseCoord = floor(uv * max(gNoiseDensity, 1.0f));
    // 時刻もfloorして連続変化を離散シードへ変え、rateが正なら1/rate秒ごとに模様全体を切り替える。
    // Y側へ1.37倍した成分を加えるのは、時間変化が対角線状の単純な平行移動に見えるのを防ぐため。
    float timeSeed = floor(gTime * max(gSeedChangeRate, 0.0f));
    float random = rand2dTo1d(noiseCoord + float2(timeSeed, timeSeed * 1.37f));
    // stepは乱数がしきい値以上のセルだけを選ぶため、しきい値を上げるほどノイズは疎になる。
    // 強度を0～1へ収め、選択セルを加算発光ではなく減光する量として使う。
    float noiseAmount = step(saturate(gNoiseThreshold), random) * saturate(gNoiseIntensity);

    // RGBだけを暗くし、ポストプロセス前後でアルファ被覆率を変化させない。
    color.rgb *= (1.0f - noiseAmount);
    return color;
}
