#include "object3d.hlsli"

// CPU側TransformationMatrixDataと同じ3行列・同じ順序の192バイト定数バッファ。
// シェーダーはrow-majorでコンパイルされ、エンジンの「ベクトル * 行列」規約をそのまま使用する。
struct TransformationMatrix {
    float32_t4x4 wVP;
    float32_t4x4 world;
    float32_t4x4 worldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

// Mesh::VertexDataとPSOのリフレクション入力レイアウトに対応する頂点形式。
struct VertexShaderInput {
    float32_t4 position : POSITION0;
    float32_t2 texCoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    // wVPはCPU側でworld * view * projectionまで合成済みなので、ここでは一度の積でクリップ空間へ移す。
    output.position = mul(input.position, gTransformationMatrix.wVP);
    output.texCoord = input.texCoord;
    // 非一様スケール下でも面に直交する方向を保つため、法線にはworldではなく逆転置行列を使う。
    // ラスタライズ後にPSでも再正規化されるが、補間へ渡す値の長さもここで安定させる。
    output.normal = normalize(mul(input.normal, (float32_t3x3) gTransformationMatrix.worldInverseTranspose));
    // 平行移動を含むworld変換後の位置を、距離減衰・視線・環境反射計算用にPSへ渡す。
    output.worldPosition = mul(input.position, gTransformationMatrix.world).xyz;
    return output;
}