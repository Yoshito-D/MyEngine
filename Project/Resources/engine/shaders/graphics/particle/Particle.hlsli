// Particle VSからPSへの補間契約。インスタンス固有値は同じプリミティブの全頂点へ複製されるため、
// 通常の透視補間を通してもuvTransformとcustomDataは実質的にインスタンス定数として届く。
struct VertexShaderOutput
{
    float4 position : SV_POSITION; // ラスタライザーへ渡すクリップ座標
    float2 texCoord : TEXCOORD0;   // 粒子単位・マテリアル単位のUV変換前座標
    float4 color : COLOR0;         // インスタンス色。リボン時は頂点アルファも乗算済み
    // 行列を4本の補間レジスターへ展開し、PS側でfloat4x4へ復元する。
    float4 uvTransform0 : TEXCOORD1;
    float4 uvTransform1 : TEXCOORD2;
    float4 uvTransform2 : TEXCOORD3;
    float4 uvTransform3 : TEXCOORD4;
    // x:寿命進行度（負値はリボン識別子）、y:粒子乱数、z:カメラ近接フェード、w:速度。
    float4 customData : TEXCOORD5;
};
