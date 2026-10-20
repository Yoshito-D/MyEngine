#include "GameEngine/pch.h"
#include "GameEngine/Utility/JsonValue.h"
#include "GameEngine/Effects/Particles/Modules/SizeOverLifetimeModule.h"

namespace GameEngine {
SizeOverLifetimeModule::SizeOverLifetimeModule() = default;

void SizeOverLifetimeModule::InitializeParticle(Particle& particle) const {
   particle.sizeOverLifetimeMultiplier = sizeMultiplier_.GetValue();
}

void SizeOverLifetimeModule::UpdateSize(Particle& particle) const {
   if (!IsEnabled()) return;

   float t = particle.GetLifeProgress();

   // ランダム倍率は生成時の値を再利用し、寿命中にサイズがちらつかないようにする。
   const float multiplier =
      sizeMultiplier_.IsRandomized() ? particle.sizeOverLifetimeMultiplier : sizeMultiplier_.Minimum();
   Vector3 size = (startSize_ + (endSize_ - startSize_) * t) * multiplier;
   particle.currentSize = size;
   particle.transform.scale = size;
}

nlohmann::json SizeOverLifetimeModule::ToJson() const {
   nlohmann::json j;
   j["enabled"] = IsEnabled();
   j["sizeMultiplier"] = sizeMultiplier_.ToJson();
   j["startSize"] = {startSize_.x, startSize_.y, startSize_.z};
   j["endSize"] = {endSize_.x, endSize_.y, endSize_.z};
   return j;
}

void SizeOverLifetimeModule::FromJson(const nlohmann::json& j) {
   if (j.contains("enabled")) {
      SetEnabled(j.at("enabled").get<bool>());
   }

   if (j.contains("sizeMultiplier")) {
      sizeMultiplier_.FromJson(j.at("sizeMultiplier"));
   }

   if (j.contains("startSize")) {
      startSize_ = JsonValue::ReadVector3(j.at("startSize"), startSize_);
   }

   if (j.contains("endSize")) {
      endSize_ = JsonValue::ReadVector3(j.at("endSize"), endSize_);
   }
}
}
