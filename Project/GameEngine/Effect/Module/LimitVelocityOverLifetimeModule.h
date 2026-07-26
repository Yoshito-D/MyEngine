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
		/// @brief 速度制限設定を既定値で初期化する
		LimitVelocityOverLifetimeModule();

		/// @brief ランダム値を粒子ごとに初期化
		void InitializeParticle(Particle& particle) const;

		/// @brief パーティクルの速度を制限
		void LimitVelocity(Particle& particle) const;
		/// @brief シミュレーション空間を考慮して速度を制限する
		void LimitVelocity(Particle& particle, const Transform& simulationTransform, bool useLocalSimulation) const;

		/// @brief 制限速度を固定値で設定する
		void SetSpeedLimit(float limit) { speedLimit_ = RandomFloat(limit, limit, false); }
		/// @brief 制限速度の代表値を取得する
		float GetSpeedLimit() const { return speedLimit_.minValue; }
		/// @brief 制限速度を乱数範囲で設定する
		void SetSpeedLimitRange(const RandomFloat& limit) { speedLimit_ = limit; }
		/// @brief 制限速度の乱数範囲を取得する
		const RandomFloat& GetSpeedLimitRange() const { return speedLimit_; }

		/// @brief 超過速度へ適用する減衰率を固定値で設定する
		void SetDampen(float dampen) { dampen_ = RandomFloat(dampen, dampen, false); }
		/// @brief 減衰率の代表値を取得する
		float GetDampen() const { return dampen_.minValue; }
		/// @brief 減衰率を乱数範囲で設定する
		void SetDampenRange(const RandomFloat& dampen) { dampen_ = dampen; }
		/// @brief 減衰率の乱数範囲を取得する
		const RandomFloat& GetDampenRange() const { return dampen_; }

		/// @copydoc ParticleModule::ToJson
		nlohmann::json ToJson() const override;
		/// @copydoc ParticleModule::FromJson
		void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI
		/// @copydoc ParticleModule::DrawInspector
		void DrawInspector() override;
#endif

	private:
		RandomFloat speedLimit_{10.0f, 10.0f, false};
		RandomFloat dampen_{0.5f, 0.5f, false};
	};
}
