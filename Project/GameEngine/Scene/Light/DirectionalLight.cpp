#include "GameEngine/pch.h"
#include <cmath>
#include <algorithm>
#include "GameEngine/Scene/Light/DirectionalLight.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"
#include "GameEngine/Math/MathUtils.h"
#include "GameEngine/Graphics/Device/GraphicsDevice.h"

namespace GameEngine {
namespace {
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;
}

void DirectionalLight::Initialize(GraphicsDevice* device) {
   if (sIsInitialized_) return;
   sDevice_ = device;
   sIsInitialized_ = true;
}

void DirectionalLight::Create(unsigned int color, const Vector3& direction, float intensity) {
   if (!sIsInitialized_)return;
   directionalLightResource_ = ResourceHelper::CreateBufferResource(sDevice_->GetDevice(), sizeof(DirectionalLightData));

   // 書き込むためのアドレスを取得
   directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

   directionalLightData_->color = ConvertUIntToColor(color);
   directionalLightData_->direction = direction;
   directionalLightData_->intensity = intensity;
   const auto requested = *directionalLightData_;
   *directionalLightData_ = {};
   ApplyIllumination(requested);
}
bool DirectionalLight::ApplyIllumination(const DirectionalLightData& illumination) {
   if (!directionalLightData_ || !std::isfinite(illumination.color.x) || !std::isfinite(illumination.color.y) || !std::isfinite(illumination.color.z) || !std::isfinite(illumination.color.w) || !std::isfinite(illumination.intensity) || !std::isfinite(illumination.direction.x) || !std::isfinite(illumination.direction.y) || !std::isfinite(illumination.direction.z)) return false;
   auto data = illumination;
   data.intensity = std::max(data.intensity, 0.0f);
   const float scale = std::max({ std::abs(data.direction.x), std::abs(data.direction.y), std::abs(data.direction.z) });
   data.direction = scale > 0.0f ? (data.direction / scale).Normalize() : Vector3{ 0.0f, -1.0f, 0.0f };
   *directionalLightData_ = data;
   return true;
}

}
