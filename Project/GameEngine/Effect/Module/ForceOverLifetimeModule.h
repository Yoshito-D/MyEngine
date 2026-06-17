#pragma once
#include "ParticleModule.h"
#include "MainModule.h"
#include "Utility/VectorMath.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
// ============================================================
// Force over Lifetime Module
// ============================================================
class ForceOverLifetimeModule : public ParticleModule {
public:
   ForceOverLifetimeModule();

   /// @brief ランダム値を粒子ごとに初期化
   void InitializeParticle(Particle& particle) const;

   /// @brief パーティクルに力を適用
   void ApplyForce(Particle& particle) const;
   void ApplyForce(Particle& particle, const Transform& simulationTransform, bool useLocalSimulation) const;

   void SetForce(const Vector3& force) { force_ = RandomVector3(force, force, false); }
   const Vector3& GetForce() const { return force_.minValue; }
   void SetForceRange(const RandomVector3& force) { force_ = force; }
   const RandomVector3& GetForceRange() const { return force_; }

   nlohmann::json ToJson() const override;
   void FromJson(const nlohmann::json& json) override;

private:
   RandomVector3 force_{ Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), false };
};
}
