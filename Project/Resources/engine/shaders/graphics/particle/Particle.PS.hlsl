#include "Particle.hlsli"

struct Material
{
    float4 color;
    float4x4 uvTransform;
    float4 renderingParams;
    float4 effectParams;
    float4 sceneParams;
    float4 projectionParams;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float4> gTexture : register(t0);
Texture2D<float4> gSceneColor : register(t1);
Texture2D<float> gSceneDepth : register(t2);
SamplerState gSampler : register(s0);

float LinearizeDepth(float depth)
{
    float nearClip = gMaterial.sceneParams.z;
    float farClip = gMaterial.sceneParams.w;
    if (gMaterial.projectionParams.x > 0.5f)
    {
        return lerp(nearClip, farClip, depth);
    }
    return nearClip * farClip / max(farClip - depth * (farClip - nearClip), 0.0001f);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4x4 particleUvTransform = float4x4(
        input.uvTransform0,
        input.uvTransform1,
        input.uvTransform2,
        input.uvTransform3);

    // UV変換 & テクスチャサンプリング
    float4 transformedUV = mul(float4(input.texCoord, 0.0f, 1.0f), particleUvTransform);
    transformedUV = mul(transformedUV, gMaterial.uvTransform);
    if (input.customData.x < 0.0f)
    {
        // 通常パーティクルのClampサンプラーを維持したまま、トレイルの長手方向だけを繰り返す。
        transformedUV.y = frac(transformedUV.y);
    }
    // Texture atlases bleed between frames through generated mipmaps, so particles sample the base mip.
    float4 textureColor = gTexture.SampleLevel(gSampler, transformedUV.xy, 0.0f);
    
     // α=0ならピクセル破棄
    if (textureColor.a <= gMaterial.renderingParams.y)
    {
        discard;
    }
    
    output.color = textureColor * input.color * gMaterial.color;

    float2 screenUV = input.position.xy / gMaterial.sceneParams.xy;
    if (gMaterial.effectParams.x > 0.5f)
    {
        float sceneDepth = gSceneDepth.SampleLevel(gSampler, screenUV, 0.0f);
        float depthSeparation = LinearizeDepth(sceneDepth) - LinearizeDepth(input.position.z);
        output.color.a *= saturate(depthSeparation / max(gMaterial.effectParams.y, 0.0001f));
    }

    // カスタムストリーム z はカメラ近接フェードとして標準マテリアルで利用する。
    output.color.a *= input.customData.z;

    float toonSteps = gMaterial.renderingParams.z;
    if (toonSteps >= 2.0f)
    {
        output.color.rgb = floor(saturate(output.color.rgb) * (toonSteps - 1.0f) + 0.5f) / (toonSteps - 1.0f);
    }
    output.color.rgb *= gMaterial.renderingParams.x;

    if (abs(gMaterial.effectParams.z) > 0.0001f)
    {
        // 通常画像は中心から放射状に歪ませ、明示指定されたRGフローマップだけを方向として解釈する。
        // 色から用途を推測しないことで、同じテクスチャがフレームごとに異なる方向へ歪むのを防ぐ。
        float2 encodedDirection = textureColor.rg * 2.0f - 1.0f;
        float2 radialDirection = input.texCoord * 2.0f - 1.0f;
        float useEncodedDirection = gMaterial.projectionParams.y > 0.5f ? 1.0f : 0.0f;
        float2 distortionDirection = lerp(radialDirection, encodedDirection, useEncodedDirection);
        float directionLengthSquared = dot(distortionDirection, distortionDirection);
        distortionDirection = directionLengthSquared > 0.000001f
            ? distortionDirection * rsqrt(directionLengthSquared)
            : float2(0.0f, 0.0f);

        float visibility = saturate(output.color.a);
        float2 inverseSceneSize = rcp(max(gMaterial.sceneParams.xy, float2(1.0f, 1.0f)));
        float2 distortionOffset = distortionDirection * gMaterial.effectParams.z * visibility * inverseSceneSize;
        float2 halfTexel = inverseSceneSize * 0.5f;
        float2 distortedUV = clamp(screenUV + distortionOffset, halfTexel, 1.0f - halfTexel);
        float4 originalScene = gSceneColor.SampleLevel(gSampler, screenUV, 0.0f);
        float4 distortedScene = gSceneColor.SampleLevel(gSampler, distortedUV, 0.0f);
        float distortionWeight = saturate(visibility * gMaterial.effectParams.w);
        float3 emissiveColor = textureColor.rgb * input.color.rgb * gMaterial.color.rgb *
            max(gMaterial.renderingParams.x - 1.0f, 0.0f) * visibility;
        output.color = lerp(originalScene, distortedScene, distortionWeight);
        output.color.rgb += emissiveColor;
        output.color.a = 1.0f;
    }
     
    if (output.color.a <= gMaterial.renderingParams.y)
    {
        discard;
    }
    
    return output;
}
