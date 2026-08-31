// SkyboxのVS/PS間契約。texCoordは2D UVではなくキューブマップを引くローカル方向ベクトルである。
struct VSOutput {
    float4 position : SV_POSITION; // 遠平面へ固定したクリップ座標
    float3 texCoord : TEXCOORD0;   // ラスタライズで補間されるキューブマップ方向
};
