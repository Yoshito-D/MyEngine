struct Well
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

struct Vertex
{
    float32_t4 position;
    float32_t2 texCoord;
    float32_t3 normal;
};

struct VertexInfluence
{
    float32_t4 weight;
    int32_t4 index;
};

struct SkinningInformation
{
    uint32_t numVertices;
};

// SkinningObject3d.VS.hlslでつくったものと同じPalette
StructuredBuffer<Well> gMatrixPalette : register(t0);
// VertexBufferViewのstream0として利用していた入力頂点
StructuredBuffer<Vertex> gInputVertices : register(t1);
// VerterBufferViewのstream1として利用していた入力インフルエンス
StructuredBuffer<VertexInfluence> gInfluences : register(t2);
// Skinning計算後の頂点データ。SkinnedVertex
RWStructuredBuffer<Vertex> gOutputVertices : register(u0);
// Skinning関するちょっとした情報
ConstantBuffer<SkinningInformation> gSkinningInformation : register(b0);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint32_t vertexIndex = DTid.x;
    if (vertexIndex < gSkinningInformation.numVertices)
    {
        // 入力頂点とインフルエンスを取得
        Vertex input = gInputVertices[vertexIndex];
        VertexInfluence influence = gInfluences[vertexIndex];
        
        // Skinning後の頂点を計算
        Vertex skinned;
        skinned.texCoord = input.texCoord;
        
        // Weightが0のInfluenceはjoint indexが未設定のままなので、無効なPalette参照を避ける。
        skinned.position = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
        skinned.normal = float32_t3(0.0f, 0.0f, 0.0f);
        float32_t totalWeight = 0.0f;
        [unroll]
        for (uint32_t i = 0; i < 4; ++i)
        {
            if (influence.weight[i] <= 0.0f || influence.index[i] < 0)
            {
                continue;
            }

            skinned.position += mul(input.position, gMatrixPalette[influence.index[i]].skeletonSpaceMatrix) * influence.weight[i];
            skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[influence.index[i]].skeletonSpaceInverseTransposeMatrix) * influence.weight[i];
            totalWeight += influence.weight[i];
        }
        skinned.position = totalWeight > 0.0f ? skinned.position : input.position;
        skinned.position.w = 1.0f;
        skinned.normal = dot(skinned.normal, skinned.normal) > 0.0f ? normalize(skinned.normal) : input.normal;
        
        // Skinning後の頂点を出力バッファに書き込む
        gOutputVertices[vertexIndex] = skinned;
    }

}
