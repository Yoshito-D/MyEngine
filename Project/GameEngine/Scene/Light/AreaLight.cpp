#include "pch.h"
#include "AreaLight.h"
#include "ResourceHelper.h"
#include "MathUtils.h"
#include "GraphicsDevice.h"

namespace GameEngine {
namespace {
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;
}

void AreaLight::Initialize(GraphicsDevice* device) {
   if (sIsInitialized_) return;
   sDevice_ = device;
   sIsInitialized_ = true;
}

void AreaLight::Create(const Vector3& position,
                       const Vector3& normal,
                       const Vector3& tangent,
                       const Vector2& size,
                       const Vector3& color,
                       float intensity) {
   if (!sIsInitialized_) return;
   areaLightResource_ = ResourceHelper::CreateBufferResource(sDevice_->GetDevice(), sizeof(AreaLightData));

   areaLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&areaLightData_));

   Vector3 normalizedNormal = Normalize(normal);
   Vector3 normalizedTangent = Normalize(tangent);

   float dotProduct = normalizedTangent.x * normalizedNormal.x + 
                      normalizedTangent.y * normalizedNormal.y + 
                      normalizedTangent.z * normalizedNormal.z;
   
   Vector3 orthogonalTangent = normalizedTangent - normalizedNormal * dotProduct;
   Vector3 finalTangent = Normalize(orthogonalTangent);

   areaLightData_->color = Vector4(color.x, color.y, color.z, 1.0f);
   areaLightData_->position = position;
   areaLightData_->intensity = intensity;
   areaLightData_->normal = normalizedNormal;
   areaLightData_->width = size.x;
   areaLightData_->tangent = finalTangent;
   areaLightData_->height = size.y;
   areaLightData_->padding = Vector3(0.0f, 0.0f, 0.0f);
   areaLightData_->padding2 = 0.0f;
}
}
