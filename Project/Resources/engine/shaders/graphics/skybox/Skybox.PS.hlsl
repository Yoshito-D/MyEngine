#include "Skybox.hlsli"

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

// CPU側SkyboxMaterialDataと一致する1レジスター（16バイト）の色変調値。
struct Material
{
    float32_t4 color;
};

ConstantBuffer<Material> gMaterial : register(b0);
// t0は2Dテクスチャではなくキューブマップ。入力方向の長さではなく向きで面とUVが決まる。
TextureCube<float4> gSkyboxTexture : register(t0);
SamplerState        gSampler       : register(s0);

PixelShaderOutput main(VSOutput input) {
    PixelShaderOutput output;
    // VSから補間された立方体方向を直接サンプルし、コンポーネント設定色で空全体を調色する。
    float32_t4 textureColor = gSkyboxTexture.Sample(gSampler, input.texCoord);
    output.color = textureColor * gMaterial.color;
    return output;
}
