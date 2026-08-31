#include "Skybox.hlsli"

// Skyboxも共通Mesh::VertexDataを使うためUVと法線を受け取るが、方向生成にはpositionだけを使う。
struct VSInput
{
    float4 position : POSITION;
    float2 texCoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

// CPU側SkyboxTransformDataと同じ192バイト配置。共通レイアウト維持のため3行列を持つが、
// Skybox固有変換を持たない現在のVSが参照するのは平行移動除去済みのwVPだけである。
struct TransformationMatrix
{
    float32_t4x4 wVP;
    float32_t4x4 world;
    float32_t4x4 worldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

VSOutput main(VSInput input) {
    VSOutput output;
    // 変換後のzをwへ置換すると除算後zが常に1（遠平面）になる。
    // PSOはLessEqualかつ深度書き込み無効なので、既描画ジオメトリを隠さず背景部分だけを埋める。
    output.position = mul(input.position, gTransformationMatrix.wVP).xyww;
    // カメラ平行移動はCPU側で除去済み。立方体のローカル位置を方向として渡すことで
    // カメラ回転には追従しつつ、移動しても空までの距離が変化しない見え方にする。
    output.texCoord = input.position.xyz;
    return output;
}
