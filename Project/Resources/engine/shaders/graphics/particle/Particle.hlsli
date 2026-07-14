struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
    float4 uvTransform0 : TEXCOORD1;
    float4 uvTransform1 : TEXCOORD2;
    float4 uvTransform2 : TEXCOORD3;
    float4 uvTransform3 : TEXCOORD4;
    float4 customData : TEXCOORD5;
};
