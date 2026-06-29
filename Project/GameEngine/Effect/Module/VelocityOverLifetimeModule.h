#pragma once
#include "ParticleModule.h"
#include "MainModule.h"
#include "Utility/VectorMath.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
	// ============================================================
	// Velocity over Lifetime Module
	// ============================================================
	class VelocityOverLifetimeModule : public ParticleModule {
	public:
		VelocityOverLifetimeModule();

		/// @brief ランダム値を粒子ごとに初期化
		void InitializeParticle(Particle& particle) const;

		/// @brief パーティクルの速度を更新
		void ApplyVelocity(Particle& particle, float deltaTime) const;
		void ApplyVelocity(Particle& particle, float deltaTime, const Transform& simulationTransform, bool useLocalSimulation) const;

		void SetLinearVelocity(const Vector3& velocity) { linearVelocity_ = RandomVector3(velocity, velocity, false); }
		const Vector3& GetLinearVelocity() const { return linearVelocity_.minValue; }
		void SetLinearVelocityRange(const RandomVector3& velocity) { linearVelocity_ = velocity; }
		const RandomVector3& GetLinearVelocityRange() const { return linearVelocity_; }

		void SetSpeedModifier(float modifier) { speedModifier_ = RandomFloat(modifier, modifier, false); }
		float GetSpeedModifier() const { return speedModifier_.minValue; }
		void SetSpeedModifierRange(const RandomFloat& modifier) { speedModifier_ = modifier; }
		const RandomFloat& GetSpeedModifierRange() const { return speedModifier_; }

		nlohmann::json ToJson() const override;
		void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI
		void DrawInspector() override;
#endif

	private:
		RandomVector3 linearVelocity_{Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), false};
		RandomFloat speedModifier_{1.0f, 1.0f, false};
	};
}
