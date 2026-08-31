#include "FullscreenTriangle.hlsli"

// C++側のOutlineCBと同じ並びを保つ。texelSizeは現在の出力解像度から毎描画更新されるため、
// thicknessを解像度に依存しない画素間隔として扱える。
struct OutlineParams
{
    float4 outlineColor;
    float2 texelSize;
    float thickness;
    float depthThreshold;
    float intensity;
};

ConstantBuffer<OutlineParams> gParams : register(b0);
Texture2D<float4> gInputTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);
SamplerState gColorSampler : register(s0);
// 深度境界を線形補間でぼかさないよう、PostProcessOutlineルートシグネチャのs1はPointサンプラーを使う。
SamplerState gDepthSampler : register(s1);

// Prewitt演算子で左右・上下の平均的な深度差を求める。係数を1/6へ正規化し、
// カーネル自体のスケールがdepthThresholdの感度を過度に増幅しないようにする。
static const float32_t kPrewittHorizontalKernel[3][3] =
{
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f }
};

static const float32_t kPrewittVerticalKernel[3][3] =
{
    { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
    { 0.0f, 0.0f, 0.0f },
    { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f }
};

// 3x3近傍を中心UVから相対指定し、thickness倍した1テクセル幅と組み合わせる。
static const float32_t2 kIndex3x3[3][3] =
{
    { float32_t2(-1, -1), float32_t2(0, -1), float32_t2(1, -1) },
    { float32_t2(-1, 0), float32_t2(0, 0), float32_t2(1, 0) },
    { float32_t2(-1, 1), float32_t2(0, 1), float32_t2(1, 1) }
};

float ConvertNdcDepthToViewZ(float ndcDepth)
{
    // 深度バッファは遠方ほど密度が低い非線形値なので、そのまま差分を取ると同じ実距離差でも
    // カメラからの距離によって輪郭感度が変わる。透視投影の式を逆にしてビュー空間距離へ戻す。
    // この変換はNear=0.1、Far=10000の投影を前提とするため、カメラのクリップ設定と一致させる必要がある。
    static const float kNearClip = 0.1f;
    static const float kFarClip = 10000.0f;
    // 遠平面付近の丸め誤差でも分母を0にせず、無限値がPrewitt差分へ伝播することを防ぐ。
    return (kNearClip * kFarClip) / max(kFarClip - ndcDepth * (kFarClip - kNearClip), 0.000001f);
}

float SampleViewZ(float2 uv)
{
    // 画面端ではUVをClampし、SampleLevel(LOD 0)で深度のmip選択や勾配依存を排除する。
    float ndcDepth = gDepthTexture.SampleLevel(gDepthSampler, saturate(uv), 0.0f);
    return ConvertNdcDepthToViewZ(ndcDepth);
}

float4 main(VSOutput input) : SV_TARGET
{
    float2 difference = float2(0.0f, 0.0f);
    // 負の太さはゼロ間隔として扱い、近傍UVが意図せず反転するのを避ける。
    float2 sampleStep = gParams.texelSize * max(gParams.thickness, 0.0f);

    // 線形化した3x3深度へ水平・垂直カーネルを畳み込み、輪郭勾配を2次元ベクトルとして蓄積する。
    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            float2 texcoord = input.texcoord + kIndex3x3[x][y] * sampleStep;
            float viewZ = SampleViewZ(texcoord);

            difference.x += viewZ * kPrewittHorizontalKernel[x][y];
            difference.y += viewZ * kPrewittVerticalKernel[x][y];
        }
    }

    // 勾配の向きには依存せず大きさだけを使い、しきい値で0～1の合成率へ正規化する。
    // intensityとoutlineColor.aを別々に掛けることで、効果全体と色側の寄与率を独立に調整できる。
    float edge = length(difference);
    float edgeWeight = saturate(edge / max(gParams.depthThreshold, 0.000001f));
    edgeWeight *= saturate(gParams.intensity) * saturate(gParams.outlineColor.a);

    // 輪郭はRGBだけへ合成し、元画像のアルファを保持して後段のブレンド契約を変えない。
    float4 baseColor = gInputTexture.Sample(gColorSampler, input.texcoord);
    float3 outlinedColor = lerp(baseColor.rgb, gParams.outlineColor.rgb, edgeWeight);
    return float4(outlinedColor, baseColor.a);
}
