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
		/// @brief 生存期間速度設定を既定値で初期化する
		VelocityOverLifetimeModule();

		/// @brief ランダム値を粒子ごとに初期化
		void InitializeParticle(Particle& particle) const;

		/// @brief パーティクルの速度を更新
		void ApplyVelocity(Particle& particle, float deltaTime) const;
		/// @brief シミュレーション空間を考慮して速度を更新する
		void ApplyVelocity(Particle& particle, float deltaTime, const Transform& simulationTransform, bool useLocalSimulation) const;

		/// @brief 加算速度を固定値で設定する
		void SetLinearVelocity(const Vector3& velocity) { linearVelocity_ = RandomVector3(velocity, velocity, false); }
		/// @brief 加算速度の代表値を取得する
		const Vector3& GetLinearVelocity() const { return linearVelocity_.minValue; }
		/// @brief 加算速度を乱数範囲で設定する
		void SetLinearVelocityRange(const RandomVector3& velocity) { linearVelocity_ = velocity; }
		/// @brief 加算速度の乱数範囲を取得する
		const RandomVector3& GetLinearVelocityRange() const { return linearVelocity_; }

		/// @brief 速度倍率を固定値で設定する
		void SetSpeedModifier(float modifier) { speedModifier_ = RandomFloat(modifier, modifier, false); }
		/// @brief 速度倍率の代表値を取得する
		float GetSpeedModifier() const { return speedModifier_.minValue; }
		/// @brief 速度倍率を乱数範囲で設定する
		void SetSpeedModifierRange(const RandomFloat& modifier) { speedModifier_ = modifier; }
		/// @brief 速度倍率の乱数範囲を取得する
		const RandomFloat& GetSpeedModifierRange() const { return speedModifier_; }

		/// @copydoc ParticleModule::ToJson
		nlohmann::json ToJson() const override;
		/// @copydoc ParticleModule::FromJson
		void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI
		/// @copydoc ParticleModule::DrawInspector
		void DrawInspector() override;
#endif

	private:
		RandomVector3 linearVelocity_{Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), false};
		RandomFloat speedModifier_{1.0f, 1.0f, false};
	};
}
