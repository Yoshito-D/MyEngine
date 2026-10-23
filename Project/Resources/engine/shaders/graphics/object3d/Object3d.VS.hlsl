#include "object3d.hlsli"

// CPU側TransformationMatrixDataと同じ3行列・同じ順序の192バイト定数バッファ。
// シェーダーはrow-majorでコンパイルされ、エンジンの「ベクトル * 行列」規約をそのまま使用する。
struct TransformationMatrix {
   float32_t4x4 wVP;                   ///< ワールド・ビュー・プロジェクション行列。
   float32_t4x4 world;                 ///< ローカル頂点をワールド座標系へ移す変換行列。
   float32_t4x4 worldInverseTranspose; ///< ワールド行列の逆行列の転置行列。
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

// Mesh::VertexDataとPSOのリフレクション入力レイアウトに対応する頂点形式。
struct VertexShaderInput {
   float32_t4 position : POSITION0; ///< 描画する頂点のローカル位置と同次座標成分。
   float32_t2 texCoord : TEXCOORD0; ///< テクスチャ座標。
   float32_t3 normal : NORMAL0;     ///< 面の向きを表す法線ベクトル。
};

VertexShaderOutput main(VertexShaderInput input) {
   VertexShaderOutput output;

   // wVPはCPU側でworld * view * projectionまで合成済みなので、ここでは一度の積でクリップ空間へ移す。
   output.position = mul(input.position, gTransformationMatrix.wVP);
   output.texCoord = input.texCoord;

   // 非一様スケール下でも面に直交する方向を保つため、法線にはworldではなく逆転置行列を使う。
   // ラスタライズ後にPSでも再正規化されるが、補間へ渡す値の長さもここで安定させる。
   output.normal = normalize(mul(input.normal, (float32_t3x3)gTransformationMatrix.worldInverseTranspose));

   // 平行移動を含むworld変換後の位置を、距離減衰・視線・環境反射計算用にPSへ渡す。
   output.worldPosition = mul(input.position, gTransformationMatrix.world).xyz;

   // テクスチャ投影をオブジェクトに固定し、惑星の移動・回転で模様が滑らないようにする。
   output.localPosition = input.position.xyz;
   output.localNormal = input.normal;
   return output;
}
