#include "pch.h"
#include "PointLight.h"
#include "ResourceHelper.h"
#include "MathUtils.h"
#include "GraphicsDevice.h"

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

   pointLightData_->color = ConvertUIntToColor(color);
   pointLightData_->position = position;
   pointLightData_->intensity = intensity;
   pointLightData_->radius = radius;
   pointLightData_->decay = decay;
}
}