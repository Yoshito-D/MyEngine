#include "GameEngine/pch.h"
#include <cstdint>
#include "GameEngine/Effects/Particles/Modules/RendererModule.h"

namespace GameEngine {
RendererModule::RendererModule() = default;

nlohmann::json RendererModule::ToJson() const {
   nlohmann::json j;
   j["enabled"] = IsEnabled();
   j["rotationSpace"] = static_cast<std::int32_t>(rotationSpace_);
   j["billboardType"] = static_cast<std::int32_t>(billboardType_);
   j["speedScale"] = speedScale_;
   j["lengthScale"] = lengthScale_;
   j["velocityStretchEnabled"] = velocityStretchEnabled_;
   j["sortMode"] = static_cast<std::int32_t>(sortMode_);
   j["cameraFadeEnabled"] = cameraFadeEnabled_;
   j["cameraFadeNear"] = cameraFadeNear_;
   j["cameraFadeFar"] = cameraFadeFar_;
   return j;
}

void RendererModule::FromJson(const nlohmann::json& j) {
   if (j.contains("enabled")) {
      SetEnabled(j["enabled"].get<bool>());
   }

   if (j.contains("rotationSpace")) {
      rotationSpace_ = static_cast<RotationSpace>(j["rotationSpace"].get<std::int32_t>());
   }

   if (j.contains("billboardType")) {
      billboardType_ = static_cast<BillboardType>(j["billboardType"].get<std::int32_t>());
   }

   if (j.contains("speedScale")) {
      speedScale_ = j["speedScale"];
   }

   if (j.contains("lengthScale")) {
      lengthScale_ = j["lengthScale"];
   }

   if (j.contains("velocityStretchEnabled")) {
      velocityStretchEnabled_ = j["velocityStretchEnabled"];
   } else {
      // 旧データのVelocityは方向合わせとストレッチが一体だったため、見た目を維持する。
      velocityStretchEnabled_ = billboardType_ == BillboardType::Velocity;
   }

   if (j.contains("sortMode")) {
      sortMode_ = static_cast<SortMode>(j["sortMode"].get<std::int32_t>());
   }

   if (j.contains("cameraFadeEnabled")) {
      cameraFadeEnabled_ = j["cameraFadeEnabled"];
   }

   if (j.contains("cameraFadeNear")) {
      SetCameraFadeNear(j["cameraFadeNear"]);
   }

   if (j.contains("cameraFadeFar")) {
      SetCameraFadeFar(j["cameraFadeFar"]);
   }
}
}
