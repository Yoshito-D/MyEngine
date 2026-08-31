// Line3DのVS/PS間契約。色はCPUのLineInstanceから両端へ同じ値が渡るため、
// 通常補間を経ても線分全体で同一となり、PSのalpha=0.05非描画判定が一様に適用される。
struct VertexShaderOutput {
    float4 position : SV_POSITION; // wVP変換済みクリップ座標
    float4 color : COLOR0;         // 非事前乗算RGBA
};