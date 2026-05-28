#pragma once
#include "ParticleModule.h"
#include "Utility/VectorMath.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
	// ============================================================
	// Velocity over Lifetime Module
	// ============================================================
	class VelocityOverLifetimeModule : public ParticleModule {
	public:
		VelocityOverLifetimeModule();

		/// @brief パーティクルの速度を更新
		void ApplyVelocity(Particle& particle, float deltaTime) const;

		void SetLinearVelocity(const Vector3& velocity) { linearVelocity_ = velocity; }
		const Vector3& GetLinearVelocity() const { return linearVelocity_; }

		void SetSpeedModifier(float modifier) { speedModifier_ = modifier; }
		float GetSpeedModifier() const { return speedModifier_; }

		nlohmann::json ToJson() const override;
		void FromJson(const nlohmann::json& json) override;

	private:
		Vector3 linearVelocity_{0.0f, 0.0f, 0.0f};
		float speedModifier_ = 1.0f;
	};
}
