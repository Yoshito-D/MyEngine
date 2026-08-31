// CPU側TextRenderer::ViewportDataと一致する16バイト定数バッファ。
// sizeはCPU/GPU契約を明示する値、inverseSizeは除算を頂点ごとに繰り返さないための値である。
struct TextViewport
{
    float2 size;
    float2 inverseSize;
};

ConstantBuffer<TextViewport> gTextViewport : register(b0);

// TextRenderer::TextVertexおよびtext_pipeline.jsonのbyte offsetと一致する48バイト頂点形式。
struct VertexShaderInput
{
    float2 position : POSITION0;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
    // xy:アトラス寸法、z:距離場レンジ、w:ビットマップ/MSDF種別。PSでカバレッジ計算に使用する。
    float4 atlasParameters : TEXCOORD1;
};

// texCoord・頂点色・アトラス情報はPSへ補間して渡し、VSは座標系変換だけを担当する。
struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
    float4 atlasParameters : TEXCOORD1;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    // CPUが生成するUI座標は左上原点・右向きX・下向きYのピクセル単位。
    // D3Dのクリップ空間へ合わせてXを[-1,+1]へ写し、Yは[+1,-1]へ反転する。
    const float2 clipPosition = float2(
        input.position.x * gTextViewport.inverseSize.x * 2.0f - 1.0f,
        1.0f - input.position.y * gTextViewport.inverseSize.y * 2.0f);
    // Text PSOは深度テストを無効化しているため、z=0の同一平面へ配置して描画順で重ねる。
    output.position = float4(clipPosition, 0.0f, 1.0f);
    output.texCoord = input.texCoord;
    output.color = input.color;
    output.atlasParameters = input.atlasParameters;
    return output;
}
