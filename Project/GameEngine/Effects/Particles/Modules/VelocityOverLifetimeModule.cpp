#include "GameEngine/pch.h"
#include "GameEngine/Effects/Particles/Modules/VelocityOverLifetimeModule.h"
#include <algorithm>
#include <cmath>

namespace GameEngine {
VelocityOverLifetimeModule::VelocityOverLifetimeModule() = default;

void VelocityOverLifetimeModule::InitializeParticle(Particle& particle) const {
   particle.velocityOverLifetimeLinearAcceleration = linearAcceleration_.GetValue();
   particle.velocityOverLifetimeSpeedGrowthRate = speedGrowthRate_.GetValue();
}

void VelocityOverLifetimeModule::ApplyVelocity(Particle& particle, float deltaTime) const {
   ApplyVelocity(particle, deltaTime, Transform{}, false);
}

void VelocityOverLifetimeModule::ApplyVelocity(
   Particle& particle, float deltaTime, const Transform& simulationTransform, bool useLocalSimulation) const {
   if (!IsEnabled() || deltaTime <= 0.0f) return;

   Vector3 linearAcceleration = linearAcceleration_.IsRandomized() ? particle.velocityOverLifetimeLinearAcceleration
                                                                   : linearAcceleration_.Minimum();

   if (useLocalSimulation) {
      // 設定値はエミッターのローカル方向として解釈し、加算前にワールド方向へ回転する。
      linearAcceleration = RotateVector(linearAcceleration, simulationTransform.GetActiveQuaternion());
   }

   particle.velocity += linearAcceleration * deltaTime;

   float speedGrowthRate =
      speedGrowthRate_.IsRandomized() ? particle.velocityOverLifetimeSpeedGrowthRate : speedGrowthRate_.Minimum();

   // 増幅率は1/sで保持し、フレーム数ではなく経過秒から倍率を求める。
   particle.velocity = particle.velocity * std::exp(speedGrowthRate * std::max(deltaTime, 0.0f));
}

nlohmann::json VelocityOverLifetimeModule::ToJson() const {
   nlohmann::json j;
   j["enabled"] = IsEnabled();
   j["linearAcceleration"] = linearAcceleration_.ToJson();
   j["speedGrowthRate"] = speedGrowthRate_.ToJson();
   return j;
}

void VelocityOverLifetimeModule::FromJson(const nlohmann::json& j) {
   if (j.contains("enabled")) {
      SetEnabled(j["enabled"].get<bool>());
   }

   if (j.contains("linearAcceleration")) {
      linearAcceleration_.FromJson(j["linearAcceleration"]);
   }

   if (j.contains("speedGrowthRate")) {
      speedGrowthRate_.FromJson(j["speedGrowthRate"]);
   }
}
}
