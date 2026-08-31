#include "Line3d.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    output.color = input.color;

    // 既存仕様ではalpha=0.05の線分を非描画として扱う。
    // VSは同じインスタンス色を両端へ複製するため、この判定は線分全体へ一様に適用される。
    // discardにより色だけでなく、深度書き込み有効のLine PSOへ不要な深度も残さない。
    if (output.color.a == 0.05f)
    {
        discard;
    }
    
    return output;
}
