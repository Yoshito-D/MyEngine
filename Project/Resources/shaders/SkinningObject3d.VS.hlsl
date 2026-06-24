struct Well {
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};
StructuredBuffer<Well> gMatrixPalette : register(t0);

struct TransformationMatrix{
    float32_t4x4 wVP;
    float32_t4x4 world;
    float32_t4x4 worldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput {
    float32_t4 position : POSITION0;
    float32_t2 texCoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 weights : WEIGHT0;
    int32_t4 index : INDEX0;
};

struct VertexShaderOutput{
    float32_t4 position : SV_POSITION;
    float32_t2 texCoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : POSITION0;
};

struct Skinned {
    float32_t4 position;
    float32_t3 normal;
};

Skinned Skinning(VertexShaderInput input) {
    Skinned skinned;
    
    skinned.position = mul(input.position, gMatrixPalette[input.index.x].skeletonSpaceMatrix) * input.weights.x;
    skinned.position += mul(input.position, gMatrixPalette[input.index.y].skeletonSpaceMatrix) * input.weights.y;
    skinned.position += mul(input.position, gMatrixPalette[input.index.z].skeletonSpaceMatrix) * input.weights.z;
    skinned.position += mul(input.position, gMatrixPalette[input.index.w].skeletonSpaceMatrix) * input.weights.w;
    skinned.position.w = 1.0f;
    
    skinned.normal = mul(input.normal, (float3x3) gMatrixPalette[input.index.x].skeletonSpaceInverseTransposeMatrix) * input.weights.x;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.y].skeletonSpaceInverseTransposeMatrix) * input.weights.y;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.z].skeletonSpaceInverseTransposeMatrix) * input.weights.z;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.w].skeletonSpaceInverseTransposeMatrix) * input.weights.w;
    skinned.normal = normalize(skinned.normal);
    
    return skinned;
}

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;

    Skinned skinned = Skinning(input);

    output.position = mul(skinned.position, gTransformationMatrix.wVP);
    output.worldPosition = mul(skinned.position, gTransformationMatrix.world).xyz;
    output.texCoord = input.texCoord;
    output.normal = normalize(mul(skinned.normal, (float3x3)gTransformationMatrix.worldInverseTranspose));

    return output;
}
