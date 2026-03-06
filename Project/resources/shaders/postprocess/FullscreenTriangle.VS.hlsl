#include "FullscreenTriangle.hlsli"

VSOutput main(uint vertexID : SV_VertexID)
{
    VSOutput output;

    // 三角形を 1 枚だけで画面全体をカバー
    float32_t2 pos = float32_t2((vertexID << 1) & 2, vertexID & 2);
    output.position = float32_t4(pos * float32_t2(2, -2) + float32_t2(-1, 1), 0, 1);
    output.texcoord = pos;

    return output;
}
