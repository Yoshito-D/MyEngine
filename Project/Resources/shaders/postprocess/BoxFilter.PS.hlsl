#include "FullscreenTriangle.hlsli"

Texture2D<float4> inputTexture : register(t0);
SamplerState textureSampler : register(s0);

cbuffer BoxFilterParams : register(b0)
{
    // カーネル半径: 1=3x3, 2=5x5, 3=7x7 ...
    int kernelRadius;
};

float4 main(VSOutput input) : SV_TARGET
{
    uint width, height;
    inputTexture.GetDimensions(width, height);
    float2 texelSize = float2(rcp((float)width), rcp((float)height));

    float4 color = float4(0.0f, 0.0f, 0.0f, 0.0f);
    int sampleCount = 0;

    for (int y = -kernelRadius; y <= kernelRadius; ++y)
    {
        for (int x = -kernelRadius; x <= kernelRadius; ++x)
        {
            float2 offset = float2((float)x, (float)y) * texelSize;
            color += inputTexture.Sample(textureSampler, input.texcoord + offset);
            ++sampleCount;
        }
    }

    color /= (float)sampleCount;
    color.a = 1.0f;
    return color;
}
