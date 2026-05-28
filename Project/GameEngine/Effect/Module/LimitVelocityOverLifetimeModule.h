#pragma once
#include "ParticleModule.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
	// ============================================================
	// Limit Velocity over Lifetime Module
	// ============================================================
	class LimitVelocityOverLifetimeModule : public ParticleModule {
	public:
		LimitVelocityOverLifetimeModule();

		/// @brief パーティクルの速度を制限
		void LimitVelocity(Particle& particle) const;

		void SetSpeedLimit(float limit) { speedLimit_ = limit; }
		float GetSpeedLimit() const { return speedLimit_; }

		void SetDampen(float dampen) { dampen_ = dampen; }
		float GetDampen() const { return dampen_; }

		nlohmann::json ToJson() const override;
		void FromJson(const nlohmann::json& json) override;

	private:
		float speedLimit_ = 10.0f;
		float dampen_ = 0.5f;
	};
}
