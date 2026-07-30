cbuffer SceneTransitionConstants : register(b0)
{
    float gOpacity;
    float2 gBlurDirection;
    uint gApplyComposite;
};

Texture2D gInputTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET
{
    static const int kKernelRadius = 8;
    static const float kGaussianWeights[kKernelRadius + 1] =
    {
        0.103152619f,
        0.099978946f,
        0.091031867f,
        0.077863682f,
        0.062565226f,
        0.047226710f,
        0.033488752f,
        0.022308318f,
        0.013960189f
    };
    static const float kBlackFadeStart = 0.65f;

    const float progress = saturate(gOpacity);
    const float effectStrength = smoothstep(0.0f, 1.0f, progress);

    uint textureWidth;
    uint textureHeight;
    gInputTexture.GetDimensions(textureWidth, textureHeight);
    const float2 texelSize =
        1.0f / float2(max(textureWidth, 1u), max(textureHeight, 1u));

    float4 originalColor = gInputTexture.Sample(gSampler, uv);
    float4 blurredColor = originalColor * kGaussianWeights[0];
    for (int offsetIndex = 1; offsetIndex <= kKernelRadius; ++offsetIndex)
    {
        const float2 sampleOffset =
            gBlurDirection * texelSize * float(offsetIndex);
        const float weight = kGaussianWeights[offsetIndex];
        blurredColor += gInputTexture.Sample(gSampler, uv - sampleOffset) * weight;
        blurredColor += gInputTexture.Sample(gSampler, uv + sampleOffset) * weight;
    }

    float4 color = lerp(originalColor, blurredColor, effectStrength);
    if (gApplyComposite == 0)
    {
        return color;
    }

    const float luminance = dot(color.rgb, float3(0.2125f, 0.7154f, 0.0721f));
    color.rgb = lerp(color.rgb, luminance.xxx, effectStrength);

    // ブラーが十分に見えてから暗転を始め、シーン差し替えの瞬間は完全な黒にする。
    const float blackFade = smoothstep(kBlackFadeStart, 1.0f, progress);
    color.rgb = lerp(color.rgb, 0.0f, blackFade);
    return color;
}
