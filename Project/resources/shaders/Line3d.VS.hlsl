#include "Line3d.hlsli"

struct TransformationMatrix
{
    float4x4 wVP;
    float4x4 world;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float vertexIndex : TEXCOORD0;  // 0 or 1
    float3 start : POSITION0;       // インスタンスごとの開始点
    float3 end : POSITION1;         // インスタンスごとの終了点
    float4 color : COLOR0;          // インスタンスごとの色
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // vertexIndexが0なら開始点、1なら終了点を使用
    float3 position = lerp(input.start, input.end, input.vertexIndex);
    
    output.position = mul(float4(position, 1.0f), gTransformationMatrix.wVP);
    output.color = input.color;
    return output;
}
