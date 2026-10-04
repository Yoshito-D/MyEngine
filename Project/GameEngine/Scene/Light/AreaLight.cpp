#include "GameEngine/pch.h"
#include <cmath>
#include <algorithm>
#include "GameEngine/Scene/Light/AreaLight.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"
#include "GameEngine/Math/MathUtils.h"
#include "GameEngine/Graphics/Device/GraphicsDevice.h"

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

   *areaLightData_ = {};
   AreaLightData requested{};
   requested.color = { color.x, color.y, color.z, 1.0f };
   requested.position = position;
   requested.intensity = intensity;
   requested.normal = normal;
   requested.tangent = tangent;
   requested.width = size.x;
   requested.height = size.y;
   ApplyIllumination(requested);
}
bool AreaLight::ApplyIllumination(const AreaLightData& illumination) {
   if (!areaLightData_ || !std::isfinite(illumination.color.x) || !std::isfinite(illumination.color.y) || !std::isfinite(illumination.color.z) || !std::isfinite(illumination.color.w) || !std::isfinite(illumination.intensity) || !std::isfinite(illumination.width) || !std::isfinite(illumination.height) || !std::isfinite(illumination.position.x) || !std::isfinite(illumination.position.y) || !std::isfinite(illumination.position.z) || !std::isfinite(illumination.normal.x) || !std::isfinite(illumination.normal.y) || !std::isfinite(illumination.normal.z) || !std::isfinite(illumination.tangent.x) || !std::isfinite(illumination.tangent.y) || !std::isfinite(illumination.tangent.z)) return false;
   auto data = illumination;
   data.intensity = std::max(data.intensity, 0.0f);
   data.width = std::max(data.width, 0.0f);
   data.height = std::max(data.height, 0.0f);
   const float normalScale = std::max({ std::abs(data.normal.x), std::abs(data.normal.y), std::abs(data.normal.z) });
   data.normal = normalScale > 0.0f ? (data.normal / normalScale).Normalize() : Vector3{ 0.0f, -1.0f, 0.0f };
   const float tangentScale = std::max({ std::abs(data.tangent.x), std::abs(data.tangent.y), std::abs(data.tangent.z) });
   if (tangentScale > 0.0f) data.tangent = data.tangent / tangentScale;
   // ライト面の接線は法線成分を取り除いてから正規化する。
   data.tangent = data.tangent - data.normal * data.normal.Dot(data.tangent);
   if (data.tangent.Length() <= 0.00001f) {
      const Vector3 axis = std::abs(data.normal.x) < 0.9f ? Vector3{ 1.0f, 0.0f, 0.0f } : Vector3{ 0.0f, 0.0f, 1.0f };
      data.tangent = axis - data.normal * data.normal.Dot(axis);
   }
   data.tangent = data.tangent.Normalize();
   data.padding = {}; data.padding2 = 0.0f;
   *areaLightData_ = data;
   return true;
}

}
