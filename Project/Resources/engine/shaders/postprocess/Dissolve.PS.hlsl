#include "FullscreenTriangle.hlsli"

// C++側のDissolveCBと同じレジスター順で、進行度、境界形状、UV変換、2種類の色を受け取る。
// b0／t0／t1は専用ルートシグネチャのconstantbuffer／inputtexture／masktextureへ対応する。
cbuffer DissolveCB : register(b0)
{
    float gThreshold;
    float gEdgeWidth;
    float gEdgeIntensity;
    float gMaskContrast;
    float2 gMaskTiling;
    float2 gMaskOffset;
    float4 gEdgeColor;
    float4 gDissolveColor;
};

Texture2D gInputTexture : register(t0);
Texture2D gMaskTexture : register(t1);
SamplerState gInputSampler : register(s0);
// マスクのタイリングを継ぎ目なく繰り返すため、PostProcessDissolveのs1はWrapサンプラーを使う。
SamplerState gMaskSampler : register(s1);

float4 main(VSOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float4 baseColor = gInputTexture.Sample(gInputSampler, uv);

    // UV倍率へ正の下限を設け、CPU外から不正なゼロ／負値が来てもマスクが一点へ潰れたり反転したりしない。
    float2 maskUV = uv * max(gMaskTiling, float2(0.0001f, 0.0001f)) + gMaskOffset;
    float maskValue = gMaskTexture.Sample(gMaskSampler, maskUV).r;
    // 0.5を中心にコントラストを変更すると、平均的な境界位置を保ったまま模様の白黒差だけを強調できる。
    maskValue = saturate((maskValue - 0.5f) * gMaskContrast + 0.5f);

    // smoothstepでしきい値周辺を連続遷移させ、ハードな二値境界によるちらつきを避ける。
    // edgeMaskはしきい値からの距離を三角形状の重みに変え、遷移帯の中央だけへエッジ色を載せる。
    float width = max(gEdgeWidth, 0.0001f);
    float visible = smoothstep(gThreshold - width, gThreshold + width, maskValue);
    float edgeMask = saturate(1.0f - abs(maskValue - gThreshold) / width);
    // smoothstepだけでは幅の分だけ端点に残像が出るため、進行度0と1の可視率を明示して開始／終了を決定的にする。
    // エッジは開始直後から徐々に立ち上げ、threshold=0で画面全体へ一瞬現れることを防ぐ。
    visible = (gThreshold <= 0.0f) ? 1.0f : visible;
    visible = (gThreshold >= 1.0f) ? 0.0f : visible;
    edgeMask *= smoothstep(0.0f, 1.0f, gThreshold);
    edgeMask *= max(gEdgeIntensity, 0.0f) * gEdgeColor.a;

    // まず消失色と入力色を可視率で補間し、その上へ境界色を重ねることで2つの役割を独立させる。
    float3 color = lerp(gDissolveColor.rgb, baseColor.rgb, visible);
    color = lerp(color, gEdgeColor.rgb, saturate(edgeMask));

    // アルファも同じ可視率で補間する。RGBは強いエッジ設定でも出力範囲を越えないよう最後に飽和する。
    float alpha = lerp(gDissolveColor.a, baseColor.a, visible);
    return float4(saturate(color), alpha);
}