#include "GameEngine/pch.h"
#include <cmath>
#include <algorithm>
#include "GameEngine/Scene/Light/PointLight.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"
#include "GameEngine/Math/MathUtils.h"
#include "GameEngine/Graphics/Device/GraphicsDevice.h"

namespace GameEngine {
namespace {
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;
}

void PointLight::Initialize(GraphicsDevice* device) {
   if (sIsInitialized_) return;
   sDevice_ = device;
   sIsInitialized_ = true;
}

void PointLight::Create(unsigned int color, const Vector3& position, float intensity, float radius, float decay) {
   if (!sIsInitialized_)return;
   pointLightResource_ = ResourceHelper::CreateBufferResource(sDevice_->GetDevice(), sizeof(PointLightData));

   // 書き込むためのアドレスを取得
   pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData_));

   *pointLightData_ = {};
   pointLightData_->color = ConvertUIntToColor(color);
   pointLightData_->position = position;
   pointLightData_->intensity = intensity;
   pointLightData_->radius = radius;
   pointLightData_->decay = decay;
   const auto requested = *pointLightData_;
   *pointLightData_ = {};
   ApplyIllumination(requested);
}
bool PointLight::ApplyIllumination(const PointLightData& illumination) {
   if (!pointLightData_ || !std::isfinite(illumination.color.x) || !std::isfinite(illumination.color.y) || !std::isfinite(illumination.color.z) || !std::isfinite(illumination.color.w) || !std::isfinite(illumination.intensity) || !std::isfinite(illumination.radius) || !std::isfinite(illumination.decay) || !std::isfinite(illumination.position.x) || !std::isfinite(illumination.position.y) || !std::isfinite(illumination.position.z)) return false;
   auto data = illumination;
   data.intensity = std::max(data.intensity, 0.0f);
   data.radius = std::max(data.radius, 0.0f);
   data.decay = std::max(data.decay, 0.0f);
   data.padding[0] = data.padding[1] = 0.0f;
   *pointLightData_ = data;
   return true;
}

}