// FullscreenTriangleルートシグネチャのtextureテーブル(t0)をそのまま画面へ転送する基準パス。
Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

// SV_POSITIONはラスタライザーとの入出力契約として受け取るが、コピー処理には補間UVだけを使う。
// オーバーサイズ三角形の頂点UVは0～2であっても、画面内にラスタライズされる領域では0～1へ補間される。
float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    // 色とアルファを加工せず返し、ポストプロセス結果の最終表示や単純コピーで値域を変えない。
    float4 color = gTexture.Sample(gSampler, uv);
    return color;
}
