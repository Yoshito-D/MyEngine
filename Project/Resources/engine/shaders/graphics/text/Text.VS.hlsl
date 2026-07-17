struct TextViewport
{
    float2 size;
    float2 inverseSize;
};

ConstantBuffer<TextViewport> gTextViewport : register(b0);

struct VertexShaderInput
{
    float2 position : POSITION0;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
    float4 atlasParameters : TEXCOORD1;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
    float4 atlasParameters : TEXCOORD1;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    const float2 clipPosition = float2(
        input.position.x * gTextViewport.inverseSize.x * 2.0f - 1.0f,
        1.0f - input.position.y * gTextViewport.inverseSize.y * 2.0f);
    output.position = float4(clipPosition, 0.0f, 1.0f);
    output.texCoord = input.texCoord;
    output.color = input.color;
    output.atlasParameters = input.atlasParameters;
    return output;
}
