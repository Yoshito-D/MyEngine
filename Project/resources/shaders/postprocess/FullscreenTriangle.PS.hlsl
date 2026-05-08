Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float3 LinearToSRGB(float3 color)
{
    float3 low  = color * 12.92f;
    float3 high = 1.055f * pow(color, 1.0f / 2.4f) - 0.055f;
    return lerp(high, low, step(color, 0.0031308f));
}

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    float4 color = gTexture.Sample(gSampler, uv);
    color.rgb = LinearToSRGB(color.rgb);
    return color;
}
