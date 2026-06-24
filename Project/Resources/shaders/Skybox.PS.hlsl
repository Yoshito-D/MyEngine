#include "Skybox.hlsli"

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

struct Material
{
    float32_t4 color;
};

ConstantBuffer<Material> gMaterial : register(b0);
TextureCube<float4> gSkyboxTexture : register(t0);
SamplerState        gSampler       : register(s0);

PixelShaderOutput main(VSOutput input) {
    PixelShaderOutput output;
    float32_t4 textureColor = gSkyboxTexture.Sample(gSampler, input.texCoord);
    output.color = textureColor * gMaterial.color;
    return output;
}
