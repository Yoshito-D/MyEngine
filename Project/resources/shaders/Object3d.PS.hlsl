#include "Object3d.hlsli"
#include "Lighting.hlsli"

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

struct Camera
{
    float32_t3 worldPosition;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<Camera> gCamera : register(b1);
ConstantBuffer<LightCount> gLightCount : register(b2);

StructuredBuffer<DirectionalLight> gDirectionalLights : register(t0);
StructuredBuffer<PointLight> gPointLights : register(t1);
StructuredBuffer<SpotLight> gSpotLights : register(t2);
StructuredBuffer<AreaLight> gAreaLights : register(t3);
Texture2D<float32_t4> gTexture : register(t4);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV変換 & テクスチャサンプリング
    float32_t4 transformedUV = mul(float32_t4(input.texCoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // αテスト
    if (textureColor.a <= 0.001f)
        discard;

    // 共通データの準備
    float32_t3 normal = normalize(input.normal);
    float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

    // ライティング結果の初期化
    float3 diffuseSum = float3(0.0f, 0.0f, 0.0f);
    float3 specularSum = float3(0.0f, 0.0f, 0.0f);
    
    if (gMaterial.lightingMode != LIGHTING_NONE)
    {
        // ディレクショナルライトの処理
        for (uint dirIdx = 0; dirIdx < gLightCount.directionalLightCount; ++dirIdx)
        {
            DirectionalLight dirLight = gDirectionalLights[dirIdx];
            
            float32_t3 diffuseWeight = float32_t3(0, 0, 0);
            float32_t3 specularWeight = float32_t3(0, 0, 0);
            
            if (gMaterial.lightingMode == LIGHTING_LAMBERT)
            {
                diffuseWeight = CalculateLambert(normal, dirLight.direction);
            }
            else
            {
                diffuseWeight = CalculateHalfLambert(normal, dirLight.direction);
            }
            
            if (gMaterial.lightingMode == LIGHTING_PHONG)
            {
                specularWeight = CalculatePhongSpecular(normal, dirLight.direction, toEye, gMaterial.shininess);
            }
            else if (gMaterial.lightingMode == LIGHTING_BLINN_PHONG)
            {
                specularWeight = CalculateBlinnPhongSpecular(normal, dirLight.direction, toEye, gMaterial.shininess);
            }
            
            diffuseSum += gMaterial.color.rgb * textureColor.rgb * dirLight.color.rgb * diffuseWeight * dirLight.intensity;
            specularSum += dirLight.color.rgb * specularWeight * dirLight.intensity;
        }
        
        // ポイントライトの処理
        for (uint pointIdx = 0; pointIdx < gLightCount.pointLightCount; ++pointIdx)
        {
            PointLight pointLight = gPointLights[pointIdx];
            
            float32_t3 pointLightDir = normalize(pointLight.position - input.worldPosition);
            float pointLightDist = length(pointLight.position - input.worldPosition);
            
            float32_t3 diffuseWeight = float32_t3(0, 0, 0);
            float32_t3 specularWeight = float32_t3(0, 0, 0);
            
            if (gMaterial.lightingMode == LIGHTING_LAMBERT)
            {
                diffuseWeight = CalculateLambert(normal, -pointLightDir);
            }
            else
            {
                diffuseWeight = CalculateHalfLambert(normal, -pointLightDir);
            }
            
            if (gMaterial.lightingMode == LIGHTING_PHONG)
            {
                specularWeight = CalculatePhongSpecular(normal, pointLightDir, toEye, gMaterial.shininess);
            }
            else if (gMaterial.lightingMode == LIGHTING_BLINN_PHONG)
            {
                specularWeight = CalculateBlinnPhongSpecular(normal, pointLightDir, toEye, gMaterial.shininess);
            }
            
            float atten = CalculatePointLightAttenuation(pointLightDist, pointLight.radius, pointLight.decay);
            diffuseSum += gMaterial.color.rgb * textureColor.rgb * pointLight.color.rgb * diffuseWeight * pointLight.intensity * atten;
            specularSum += pointLight.color.rgb * specularWeight * pointLight.intensity * atten;
        }
        
        // スポットライトの処理
        for (uint spotIdx = 0; spotIdx < gLightCount.spotLightCount; ++spotIdx)
        {
            SpotLight spotLight = gSpotLights[spotIdx];
            
            float32_t3 spotLightDirectionOnSurface = normalize(input.worldPosition - spotLight.position);
            float spotLightDist = length(spotLight.position - input.worldPosition);
            
            float32_t3 diffuseWeight = float32_t3(0, 0, 0);
            float32_t3 specularWeight = float32_t3(0, 0, 0);
            
            if (gMaterial.lightingMode == LIGHTING_LAMBERT)
            {
                diffuseWeight = CalculateLambert(normal, spotLightDirectionOnSurface);
            }
            else
            {
                diffuseWeight = CalculateHalfLambert(normal, spotLightDirectionOnSurface);
            }
            
            if (gMaterial.lightingMode == LIGHTING_PHONG)
            {
                specularWeight = CalculatePhongSpecular(normal, spotLightDirectionOnSurface, toEye, gMaterial.shininess);
            }
            else if (gMaterial.lightingMode == LIGHTING_BLINN_PHONG)
            {
                specularWeight = CalculateBlinnPhongSpecular(normal, spotLightDirectionOnSurface, toEye, gMaterial.shininess);
            }
            
            float atten = CalculateSpotLightAttenuation(spotLightDist, spotLight.distance, spotLight.decay);
            float32_t cosAngle = dot(spotLightDirectionOnSurface, spotLight.direction);
            float32_t falloff = saturate((cosAngle - spotLight.cosAngle) / (spotLight.cosFalloffStart - spotLight.cosAngle));
            
            diffuseSum += gMaterial.color.rgb * textureColor.rgb * spotLight.color.rgb * diffuseWeight * spotLight.intensity * atten * falloff;
            specularSum += spotLight.color.rgb * specularWeight * spotLight.intensity * atten * falloff;
        }
        
        // エリアライトの処理
        for (uint areaIdx = 0; areaIdx < gLightCount.areaLightCount; ++areaIdx)
        {
            AreaLight areaLight = gAreaLights[areaIdx];
            
            float32_t3 specularWeight = float32_t3(0, 0, 0);
            float32_t3 diffuseWeight = CalculateAreaLightContribution(
                input.worldPosition, normal,
                areaLight.position, areaLight.normal, areaLight.tangent,
                areaLight.width, areaLight.height,
                toEye, gMaterial.shininess, gMaterial.lightingMode,
                specularWeight
            );
            
            diffuseSum += gMaterial.color.rgb * textureColor.rgb * areaLight.color.rgb * diffuseWeight * areaLight.intensity;
            specularSum += areaLight.color.rgb * specularWeight * areaLight.intensity;
        }
    }
    else
    {
        // LIGHTING_NONE
        diffuseSum = gMaterial.color.rgb * textureColor.rgb;
    }

    // 最終合成
    output.color.rgb = diffuseSum + specularSum;
    output.color.a = gMaterial.color.a * textureColor.a;

    // 最終αチェック
    if (output.color.a <= 0.001f)
        discard;

    return output;
}