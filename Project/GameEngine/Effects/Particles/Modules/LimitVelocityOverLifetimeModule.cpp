#include "GameEngine/pch.h"
#include "GameEngine/Effects/Particles/Modules/LimitVelocityOverLifetimeModule.h"
#include <algorithm>
#include <cmath>

namespace GameEngine {
LimitVelocityOverLifetimeModule::LimitVelocityOverLifetimeModule() = default;

void LimitVelocityOverLifetimeModule::InitializeParticle(Particle& particle) const {
   particle.limitVelocitySpeedLimit = speedLimit_.GetValue();
   particle.limitVelocityDampingRate = dampingRate_.GetValue();
}

void LimitVelocityOverLifetimeModule::LimitVelocity(Particle& particle, float deltaTime) const {
   LimitVelocity(particle, deltaTime, Transform{}, false);
}

void LimitVelocityOverLifetimeModule::LimitVelocity(Particle& particle, float deltaTime, const Transform&, bool) const {
   if (!IsEnabled() || deltaTime <= 0.0f) return;

   float speed = particle.velocity.Length();
   float speedLimit =
      std::max(0.0f, speedLimit_.IsRandomized() ? particle.limitVelocitySpeedLimit : speedLimit_.Minimum());
   float dampingRate = dampingRate_.IsRandomized() ? particle.limitVelocityDampingRate : dampingRate_.Minimum();

   if (speed > speedLimit) {
      // 超過速度を秒基準で指数減衰させ、処理回数による減衰量の変化と速度反転を防ぐ。
      const float response = -std::expm1(-std::max(0.0f, dampingRate) * deltaTime);
      const float excess = speed - speedLimit;
      particle.velocity = particle.velocity * (1.0f - response * excess / speed);
   }
}

nlohmann::json LimitVelocityOverLifetimeModule::ToJson() const {
   nlohmann::json j;
   j["enabled"] = IsEnabled();
   j["speedLimit"] = speedLimit_.ToJson();
   j["dampingRate"] = dampingRate_.ToJson();
   return j;
}

void LimitVelocityOverLifetimeModule::FromJson(const nlohmann::json& j) {
   if (j.contains("enabled")) {
      SetEnabled(j["enabled"].get<bool>());
   }

   if (j.contains("speedLimit")) {
      speedLimit_.FromJson(j["speedLimit"]);
   }

   if (j.contains("dampingRate")) {
      dampingRate_.FromJson(j["dampingRate"]);
   }
}
}
