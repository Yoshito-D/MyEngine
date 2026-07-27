cbuffer SceneTransitionConstants : register(b0)
{
    float gOpacity;
    float3 gPadding;
};

Texture2D gInputTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    float4 color = gInputTexture.Sample(gSampler, uv);
    color.rgb = lerp(color.rgb, float3(0.0f, 0.0f, 0.0f), saturate(gOpacity));
    return color;
}
