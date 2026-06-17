#pragma once
#include "ParticleModule.h"
#include "MainModule.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
	// ============================================================
	// Limit Velocity over Lifetime Module
	// ============================================================
	class LimitVelocityOverLifetimeModule : public ParticleModule {
	public:
		LimitVelocityOverLifetimeModule();

		/// @brief ランダム値を粒子ごとに初期化
		void InitializeParticle(Particle& particle) const;

		/// @brief パーティクルの速度を制限
		void LimitVelocity(Particle& particle) const;
		void LimitVelocity(Particle& particle, const Transform& simulationTransform, bool useLocalSimulation) const;

		void SetSpeedLimit(float limit) { speedLimit_ = RandomFloat(limit, limit, false); }
		float GetSpeedLimit() const { return speedLimit_.minValue; }
		void SetSpeedLimitRange(const RandomFloat& limit) { speedLimit_ = limit; }
		const RandomFloat& GetSpeedLimitRange() const { return speedLimit_; }

		void SetDampen(float dampen) { dampen_ = RandomFloat(dampen, dampen, false); }
		float GetDampen() const { return dampen_.minValue; }
		void SetDampenRange(const RandomFloat& dampen) { dampen_ = dampen; }
		const RandomFloat& GetDampenRange() const { return dampen_; }

		nlohmann::json ToJson() const override;
		void FromJson(const nlohmann::json& json) override;

	private:
		RandomFloat speedLimit_{10.0f, 10.0f, false};
		RandomFloat dampen_{0.5f, 0.5f, false};
	};
}
