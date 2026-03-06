#include "pch.h"
#include "DirectionalLight.h"
#include "ResourceHelper.h"
#include "MathUtils.h"
#include "GraphicsDevice.h"

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
}
}
