#include "Object3d.hlsli"
#include "Lighting.hlsli"

struct PixelShaderOutput {
   float32_t4 color : SV_TARGET0;
};

struct Camera {
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
TextureCube<float32_t4> gEnviromentTexture : register(t5);
SamplerState gSampler : register(s0);

#ifdef OBJECT3D_TRIPLANAR
// 専用パイプラインのmaterialParametersと同じ順序・オフセットの32バイト定数バッファ。
struct TriplanarParameters {
   float triplanarScale;
   float triplanarSharpness;
   float triplanarSampleX;
   float triplanarSampleY;
   float triplanarSampleZ;
};

ConstantBuffer<TriplanarParameters> gTriplanar : register(b3);

/// @brief 各投影面にもマテリアルのUVスケール・回転・移動を適用する。
float32_t2 TransformProjectedUV(float32_t2 uv) {
   return mul(float32_t4(uv, 0.0f, 1.0f), gMaterial.uvTransform).xy;
}

/// @brief ローカル空間の3平面から投影し、UV球の極へのテクスチャ集中を避ける。
float32_t4 SampleTriplanar(float32_t3 localPosition, float32_t3 localNormal) {
   float32_t3 enabled = float32_t3(gTriplanar.triplanarSampleX > 0.5f ? 1.0f : 0.0f,
      gTriplanar.triplanarSampleY > 0.5f ? 1.0f : 0.0f, gTriplanar.triplanarSampleZ > 0.5f ? 1.0f : 0.0f);
   float enabledCount = enabled.x + enabled.y + enabled.z;

   // 全方向オフは色テクスチャを読まず、呼び出し元のマテリアル色を使用する。
   [branch] if (enabledCount == 0.0f) {
      return float32_t4(1.0f, 1.0f, 1.0f, 1.0f);
   }

   float32_t3 position = localPosition * max(gTriplanar.triplanarScale, 0.001f);
   float32_t3 normal = localNormal * rsqrt(max(dot(localNormal, localNormal), 0.00000001f));

   // 重みを正規化して明るさを保つ。指数の上限で中間方向のアンダーフローを避ける。
   float32_t3 weights = pow(abs(normal), clamp(gTriplanar.triplanarSharpness, 1.0f, 32.0f)) * enabled;
   float weightSum = weights.x + weights.y + weights.z;

   // 法線がゼロ、または有効な全方向と直交するときも、有効な方向だけを等しく混ぜる。
   weights = weightSum > 0.0f ? weights / weightSum : enabled / enabledCount;

   float32_t2 uvX = TransformProjectedUV(position.zy);
   float32_t2 uvY = TransformProjectedUV(position.xz);
   float32_t2 uvZ = TransformProjectedUV(position.xy);

   // 分岐前に勾配を求め、方向を切り替えてもミップ選択を安定させる。
   float32_t2 dxX = ddx(uvX), dyX = ddy(uvX);
   float32_t2 dxY = ddx(uvY), dyY = ddy(uvY);
   float32_t2 dxZ = ddx(uvZ), dyZ = ddy(uvZ);
   float32_t4 color = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);

   // 重みを0にするだけでなく、無効な方向のテクスチャ取得自体を省く。
   [branch] if (enabled.x > 0.0f) {
      color += gTexture.SampleGrad(gSampler, uvX, dxX, dyX) * weights.x;
   }

   [branch] if (enabled.y > 0.0f) {
      color += gTexture.SampleGrad(gSampler, uvY, dxY, dyY) * weights.y;
   }

   [branch] if (enabled.z > 0.0f) {
      color += gTexture.SampleGrad(gSampler, uvZ, dxZ, dyZ) * weights.z;
   }

   return color;
}
#endif

PixelShaderOutput main(VertexShaderOutput input) {
   PixelShaderOutput output;

#ifdef OBJECT3D_TRIPLANAR
   float32_t4 textureColor = SampleTriplanar(input.localPosition, input.localNormal);
#else
   // UV変換 & テクスチャサンプリング
   float32_t4 transformedUV = mul(float32_t4(input.texCoord, 0.0f, 1.0f), gMaterial.uvTransform);
   float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
#endif
   float32_t3 albedo = gMaterial.color.rgb * textureColor.rgb;

   // αテスト
   if (textureColor.a <= 0.001f) {
      discard;
   }

   // 共通データの準備
   float32_t3 normal = normalize(input.normal);
   float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

   // ライティング結果の初期化
   float3 diffuseSum = float3(0.0f, 0.0f, 0.0f);
   float3 specularSum = float3(0.0f, 0.0f, 0.0f);

   if (gMaterial.lightingMode != LIGHTING_NONE) {
      // ディレクショナルライトの処理
      for (uint dirIdx = 0; dirIdx < gLightCount.directionalLightCount; ++dirIdx) {
         DirectionalLight dirLight = gDirectionalLights[dirIdx];

         float32_t3 diffuseWeight = float32_t3(0, 0, 0);
         float32_t3 specularWeight = float32_t3(0, 0, 0);

         if (gMaterial.lightingMode == LIGHTING_LAMBERT) {
            diffuseWeight = CalculateLambert(normal, dirLight.direction);
         } else {
            diffuseWeight = CalculateHalfLambert(normal, dirLight.direction);
         }

         if (gMaterial.lightingMode == LIGHTING_PHONG) {
            specularWeight = CalculatePhongSpecular(normal, dirLight.direction, toEye, gMaterial.shininess);
         } else if (gMaterial.lightingMode == LIGHTING_BLINN_PHONG) {
            specularWeight = CalculateBlinnPhongSpecular(normal, dirLight.direction, toEye, gMaterial.shininess);
         }

         diffuseSum += gMaterial.color.rgb * textureColor.rgb * dirLight.color.rgb * diffuseWeight * dirLight.intensity;
         specularSum += dirLight.color.rgb * specularWeight * dirLight.intensity;
      }

      // ポイントライトの処理
      for (uint pointIdx = 0; pointIdx < gLightCount.pointLightCount; ++pointIdx) {
         PointLight pointLight = gPointLights[pointIdx];

         float32_t3 pointLightDir = normalize(pointLight.position - input.worldPosition);
         float pointLightDist = length(pointLight.position - input.worldPosition);

         float32_t3 diffuseWeight = float32_t3(0, 0, 0);
         float32_t3 specularWeight = float32_t3(0, 0, 0);

         if (gMaterial.lightingMode == LIGHTING_LAMBERT) {
            diffuseWeight = CalculateLambert(normal, -pointLightDir);
         } else {
            diffuseWeight = CalculateHalfLambert(normal, -pointLightDir);
         }

         if (gMaterial.lightingMode == LIGHTING_PHONG) {
            specularWeight = CalculatePhongSpecular(normal, pointLightDir, toEye, gMaterial.shininess);
         } else if (gMaterial.lightingMode == LIGHTING_BLINN_PHONG) {
            specularWeight = CalculateBlinnPhongSpecular(normal, pointLightDir, toEye, gMaterial.shininess);
         }

         float atten = CalculatePointLightAttenuation(pointLightDist, pointLight.radius, pointLight.decay);
         diffuseSum += gMaterial.color.rgb * textureColor.rgb * pointLight.color.rgb * diffuseWeight *
                       pointLight.intensity * atten;
         specularSum += pointLight.color.rgb * specularWeight * pointLight.intensity * atten;
      }

      // スポットライトの処理
      for (uint spotIdx = 0; spotIdx < gLightCount.spotLightCount; ++spotIdx) {
         SpotLight spotLight = gSpotLights[spotIdx];

         float32_t3 spotLightDirectionOnSurface = normalize(input.worldPosition - spotLight.position);
         float spotLightDist = length(spotLight.position - input.worldPosition);

         float32_t3 diffuseWeight = float32_t3(0, 0, 0);
         float32_t3 specularWeight = float32_t3(0, 0, 0);

         if (gMaterial.lightingMode == LIGHTING_LAMBERT) {
            diffuseWeight = CalculateLambert(normal, spotLightDirectionOnSurface);
         } else {
            diffuseWeight = CalculateHalfLambert(normal, spotLightDirectionOnSurface);
         }

         if (gMaterial.lightingMode == LIGHTING_PHONG) {
            specularWeight = CalculatePhongSpecular(normal, spotLightDirectionOnSurface, toEye, gMaterial.shininess);
         } else if (gMaterial.lightingMode == LIGHTING_BLINN_PHONG) {
            specularWeight =
               CalculateBlinnPhongSpecular(normal, spotLightDirectionOnSurface, toEye, gMaterial.shininess);
         }

         float atten = CalculateSpotLightAttenuation(spotLightDist, spotLight.distance, spotLight.decay);
         float32_t cosAngle = dot(spotLightDirectionOnSurface, spotLight.direction);
         float32_t falloff =
            saturate((cosAngle - spotLight.cosAngle) / (spotLight.cosFalloffStart - spotLight.cosAngle));

         diffuseSum += gMaterial.color.rgb * textureColor.rgb * spotLight.color.rgb * diffuseWeight *
                       spotLight.intensity * atten * falloff;
         specularSum += spotLight.color.rgb * specularWeight * spotLight.intensity * atten * falloff;
      }

      // エリアライトの処理
      for (uint areaIdx = 0; areaIdx < gLightCount.areaLightCount; ++areaIdx) {
         AreaLight areaLight = gAreaLights[areaIdx];

         float32_t3 specularWeight = float32_t3(0, 0, 0);
         float32_t3 diffuseWeight = CalculateAreaLightContribution(input.worldPosition, normal, areaLight.position,
            areaLight.normal, areaLight.tangent, areaLight.width, areaLight.height, toEye, gMaterial.shininess,
            gMaterial.lightingMode, specularWeight);

         diffuseSum +=
            gMaterial.color.rgb * textureColor.rgb * areaLight.color.rgb * diffuseWeight * areaLight.intensity;
         specularSum += areaLight.color.rgb * specularWeight * areaLight.intensity;
      }
   } else {
      // LIGHTING_NONE
      diffuseSum = gMaterial.color.rgb * textureColor.rgb;
   }

   // 合成
   output.color.rgb = diffuseSum + specularSum;

   if (gMaterial.lightingMode != LIGHTING_NONE) {
      output.color.rgb += CalculateFillLight(albedo, gMaterial.fillLightColor.rgb, gMaterial.fillLightIntensity);
      output.color.rgb += CalculateRimLight(
         normal, toEye, gMaterial.rimLightColor.rgb, gMaterial.rimLightIntensity, gMaterial.rimLightPower);
   }

   // 環境マップ反射
   if (gMaterial.lightingMode != LIGHTING_NONE) {
      float32_t3 cameraToPosition = normalize(input.worldPosition - gCamera.worldPosition);
      float32_t3 reflectedVector = reflect(cameraToPosition, normalize(input.normal));
      float32_t4 environmentColor = gEnviromentTexture.Sample(gSampler, reflectedVector);

      output.color.rgb += (environmentColor.rgb * gMaterial.environmentCoefficient);
   }

   output.color.a = gMaterial.color.a * textureColor.a;

   // 最終αチェック
   if (output.color.a <= 0.001f) {
      discard;
   }

   return output;
}
