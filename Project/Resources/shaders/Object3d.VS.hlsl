#include "object3d.hlsli"

struct TransformationMatrix {
    float32_t4x4 wVP;
    float32_t4x4 world;
    float32_t4x4 worldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput {
    float32_t4 position : POSITION0;
    float32_t2 texCoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    output.position = mul(input.position, gTransformationMatrix.wVP);
    output.texCoord = input.texCoord;
    output.normal = normalize(mul(input.normal, (float32_t3x3) gTransformationMatrix.worldInverseTranspose));
    output.worldPosition = mul(input.position, gTransformationMatrix.world).xyz;
    return output;
}