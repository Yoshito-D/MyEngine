#include "GameEngine/pch.h"
#include <cmath>
#include <algorithm>
#include "GameEngine/Scene/Light/SpotLight.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"
#include "GameEngine/Math/MathUtils.h"
#include "GameEngine/Graphics/Device/GraphicsDevice.h"

namespace GameEngine {
namespace {
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;
}

void SpotLight::Initialize(GraphicsDevice* device) {
   if (sIsInitialized_) return;
   sDevice_ = device;
   sIsInitialized_ = true;
}

void SpotLight::Create(unsigned int color, const Vector3& position, float intensity, const Vector3& direction, float distance, float decay, float cosAngle, float cosFalloffStart) {
   if (!sIsInitialized_) return;
   spotLightResource_ = ResourceHelper::CreateBufferResource(sDevice_->GetDevice(), sizeof(SpotLightData));

   // 書き込むためのアドレスを取得
   spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData_));

   spotLightData_->color = ConvertUIntToColor(color);
   spotLightData_->position = position;
   spotLightData_->intensity = intensity;
   spotLightData_->direction = direction;
   spotLightData_->distance = distance;
   spotLightData_->decay = decay;
   spotLightData_->cosAngle = cosAngle;
   spotLightData_->cosFalloffStart = cosFalloffStart;
   spotLightData_->padding = 0.0f;
   const auto requested = *spotLightData_;
   *spotLightData_ = {};
   ApplyIllumination(requested);
}
bool SpotLight::ApplyIllumination(const SpotLightData& illumination) {
   if (!spotLightData_ || !std::isfinite(illumination.color.x) || !std::isfinite(illumination.color.y) || !std::isfinite(illumination.color.z) || !std::isfinite(illumination.color.w) || !std::isfinite(illumination.intensity) || !std::isfinite(illumination.distance) || !std::isfinite(illumination.decay) || !std::isfinite(illumination.cosAngle) || !std::isfinite(illumination.cosFalloffStart) || !std::isfinite(illumination.position.x) || !std::isfinite(illumination.position.y) || !std::isfinite(illumination.position.z) || !std::isfinite(illumination.direction.x) || !std::isfinite(illumination.direction.y) || !std::isfinite(illumination.direction.z)) return false;
   auto data = illumination;
   data.intensity = std::max(data.intensity, 0.0f);
   data.distance = std::max(data.distance, 0.0f);
   data.decay = std::max(data.decay, 0.0f);
   const float scale = std::max({ std::abs(data.direction.x), std::abs(data.direction.y), std::abs(data.direction.z) });
   data.direction = scale > 0.0f ? (data.direction / scale).Normalize() : Vector3{ 0.0f, -1.0f, 0.0f };
   data.cosAngle = std::clamp(data.cosAngle, -1.0f, 1.0f);
   data.cosFalloffStart = std::clamp(data.cosFalloffStart, data.cosAngle, 1.0f);
   data.padding = 0.0f;
   *spotLightData_ = data;
   return true;
}

}
