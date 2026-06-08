#pragma once
#include "ParticleModule.h"
#include "Utility/VectorMath.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
// ============================================================
// Force over Lifetime Module
// ============================================================
class ForceOverLifetimeModule : public ParticleModule {
public:
   ForceOverLifetimeModule();

   /// @brief パーティクルに力を適用
   void ApplyForce(Particle& particle) const;

   void SetForce(const Vector3& force) { force_ = force; }
   const Vector3& GetForce() const { return force_; }

   nlohmann::json ToJson() const override;
   void FromJson(const nlohmann::json& json) override;

private:
   Vector3 force_{ 0.0f, 0.0f, 0.0f };
};
}
