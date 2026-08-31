#include "FullscreenTriangle.hlsli"

// 汎用PostProcessルートシグネチャのinputtextureと線形Clampサンプラーを使う。
Texture2D sceneTex : register(t0);
SamplerState samLinear : register(s0);

// C++側のParamsCBと同じ32バイト配置。pixelShiftは画素数で指定し、texSizeでUV量へ変換する。
// texSizeの各成分は除数になるため正値が必須で、現在のC++実装は基準解像度1280x720を渡している。
cbuffer ParamsCB : register(b0)
{
    float2 center; // 中心UV
    float pixelShift;
    int32_t useFixedDirection; // 0 or 1
    float2 fixedDirection; // 固定方向
    float2 texSize;
};

float4 main(VSOutput input) : SV_TARGET
{
    float2 dir;
    float2 shiftUV;

    // 固定方向モードは画面全体を同じ向きへ分離する演出用。正規化することで方向ベクトルの長さが
    // pixelShiftの見かけの強さへ混ざらないようにするため、fixedDirectionには非ゼロ値を渡す。
    if (useFixedDirection != 0)
    {
        dir = normalize(fixedDirection);
        shiftUV = dir * (pixelShift / texSize); // ピクセル→UV変換
    }
    else
    {
        // 中心から外向きの方向を選び、距離に比例して分離量を増やすことでレンズ周辺ほど強い
        // 放射状の色収差を作る。中心へ近づくほど距離項が小さくなり、色ずれも目立たなくなる。
        dir = normalize(input.texcoord - center);
        float dist = length(input.texcoord - center);
        shiftUV = dir * (pixelShift / texSize) * dist;
    }

    // 緑を基準位置に残し、赤と青を同じ量だけ反対方向から採取すると、輪郭の両側へ対称な色ずれが生じる。
    // アルファは中心サンプルを使い、色収差だけでオブジェクトの透明境界が広がらないようにする。
    float r = sceneTex.Sample(samLinear, input.texcoord + shiftUV).r;
    float g = sceneTex.Sample(samLinear, input.texcoord).g;
    float b = sceneTex.Sample(samLinear, input.texcoord - shiftUV).b;
    float a = sceneTex.Sample(samLinear, input.texcoord).a;

    return float4(r, g, b, a);
}
