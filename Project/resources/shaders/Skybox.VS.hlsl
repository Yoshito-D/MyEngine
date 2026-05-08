#include "Skybox.hlsli"

struct VSInput
{
    float4 position : POSITION;
    float2 texCoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct TransformationMatrix
{
    float32_t4x4 wVP;
    float32_t4x4 world;
    float32_t4x4 worldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(input.position, gTransformationMatrix.wVP).xyww;
    output.texCoord = input.position.xyz;
    return output;
}
