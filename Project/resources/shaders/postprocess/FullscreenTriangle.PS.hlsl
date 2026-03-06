Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    return gTexture.Sample(gSampler, uv);
}
