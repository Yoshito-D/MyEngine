#include "pch.h"
#include "SpotLight.h"
#include "ResourceHelper.h"
#include "MathUtils.h"
#include "GraphicsDevice.h"

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
}
}
