#include "FullscreenTriangle.hlsli"

// C++側のAntiAliasingCBと同じ順序で4つのfloatを並べる。
// b0とt0はPostProcessルートシグネチャのconstantbuffer／inputtextureへ対応する。
struct AntiAliasingParams
{
    float contrastThreshold;
    float relativeThreshold;
    float subpixelBlending;
    float edgeSearchSteps;
};

ConstantBuffer<AntiAliasingParams> gParams : register(b0);
Texture2D<float4> gInputTexture : register(t0);
SamplerState gSampler : register(s0);

// RGBを知覚上の明るさへ集約し、色相差ではなく明暗境界をエッジ判定へ使う。
// 緑を強く、青を弱く扱う重みは人間の視覚感度に近い輝度近似である。
static const float3 kLuma = float3(0.299f, 0.587f, 0.114f);

float Luminance(float3 color)
{
    return dot(color, kLuma);
}

float3 SampleColor(float2 uv)
{
    // 方向探索のタップが画面外へ出ても反対端を読まないよう、UVを明示的に有効範囲へ収める。
    return gInputTexture.SampleLevel(gSampler, saturate(uv), 0.0f).rgb;
}

float4 main(VSOutput input) : SV_TARGET
{
    // 画素単位の探索距離を解像度非依存のUVへ変換する。
    uint width;
    uint height;
    gInputTexture.GetDimensions(width, height);
    float2 texelSize = float2(rcp((float)width), rcp((float)height));

    // 中心と対角4点だけで局所輝度範囲とエッジ方向を近似し、サンプル数を抑える。
    float4 sampleM = gInputTexture.SampleLevel(gSampler, input.texcoord, 0.0f);
    float3 colorM = sampleM.rgb;
    float3 colorNW = SampleColor(input.texcoord + texelSize * float2(-1.0f, -1.0f));
    float3 colorNE = SampleColor(input.texcoord + texelSize * float2(1.0f, -1.0f));
    float3 colorSW = SampleColor(input.texcoord + texelSize * float2(-1.0f, 1.0f));
    float3 colorSE = SampleColor(input.texcoord + texelSize * float2(1.0f, 1.0f));

    float lumaM = Luminance(colorM);
    float lumaNW = Luminance(colorNW);
    float lumaNE = Luminance(colorNE);
    float lumaSW = Luminance(colorSW);
    float lumaSE = Luminance(colorSE);

    // 暗部では絶対しきい値、明部では最大輝度に比例するしきい値を優先する。
    // これにより微小なノイズをエッジとしてぼかすことと、明部だけ過敏になることを避ける。
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;
    float edgeThreshold = max(gParams.contrastThreshold, lumaMax * gParams.relativeThreshold);
    if (lumaRange < edgeThreshold)
    {
        return sampleM;
    }

    // 対角4点から得た輝度勾配を90度回した向きを作り、境界を横切らず沿う方向へサンプルする。
    float2 direction;
    direction.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    direction.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    static const float kReduceMul = 1.0f / 8.0f;
    static const float kReduceMin = 1.0f / 128.0f;
    // 局所コントラストが小さいときに方向ベクトルを過度に拡大しないよう、平均輝度由来の項と
    // 固定下限を分母へ加える。min成分を基準に正規化するのは斜め方向の探索幅を安定させるため。
    float directionReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25f * kReduceMul), kReduceMin);
    float inverseDirectionAdjustment = rcp(min(abs(direction.x), abs(direction.y)) + directionReduce);

    float maxSearch = max(gParams.edgeSearchSteps, 1.0f);
    // C++側のedgeSearchStepsはループ回数ではなく、推定方向へ許可する最大画素距離として働く。
    direction = clamp(direction * inverseDirectionAdjustment, -maxSearch, maxSearch) * texelSize;

    // 内側2点の候補はエッジ近傍だけを平均し、細部を比較的残す。
    float3 colorA =
        0.5f * (
            SampleColor(input.texcoord + direction * (1.0f / 3.0f - 0.5f)) +
            SampleColor(input.texcoord + direction * (2.0f / 3.0f - 0.5f)));

    // 両端も含む広い候補はジャギーをより強く平滑化するが、境界をまたぐ危険がある。
    float3 colorB =
        colorA * 0.5f +
        0.25f * (
            SampleColor(input.texcoord + direction * -0.5f) +
            SampleColor(input.texcoord + direction * 0.5f));

    // 広い候補の輝度が元の局所範囲を外れた場合は、別領域を混ぜたと判断して狭い候補へ戻す。
    float lumaB = Luminance(colorB);
    float3 fxaaColor = (lumaB < lumaMin || lumaB > lumaMax) ? colorA : colorB;

    // 最終補間率を飽和させ、CPU APIから範囲外値が渡っても過補間による反転色を作らない。
    float subpixelAmount = saturate(gParams.subpixelBlending);
    float3 result = lerp(colorM, fxaaColor, subpixelAmount);
    return float4(result, sampleM.a);
}