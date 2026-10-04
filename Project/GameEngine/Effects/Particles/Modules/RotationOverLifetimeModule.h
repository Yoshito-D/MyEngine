#pragma once
#include "GameEngine/Effects/Particles/Modules/ParticleModule.h"
#include "GameEngine/Math/VectorMath.h"
#include "GameEngine/Math/MathUtils.h"
#include "GameEngine/Effects/Particles/Modules/MainModule.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
	// ============================================================
	// Rotation over Lifetime Module (ランダム対応)
	// ============================================================
	class RotationOverLifetimeModule : public ParticleModule {
	public:
		/// @brief 生存期間回転設定を既定値で初期化する
		RotationOverLifetimeModule();

		/// @brief パーティクルの回転を更新
		void UpdateRotation(Particle& particle, float deltaTime) const;
		/// @brief 角速度の乱数範囲下限を取得する
		Vector3 GetAngularVelocityMin() const { return angularVelocity_.Minimum(); }
		/// @brief 角速度の乱数範囲上限を取得する
		Vector3 GetAngularVelocityMax() const { return angularVelocity_.Maximum(); }
		/// @brief 角速度のランダム化が有効か取得する
		bool GetAngularVelocityRandomize() const { return angularVelocity_.IsRandomized(); }

		/// @brief ランダムな角速度を取得
		Vector3 GetRandomAngularVelocity() const;

		/// @copydoc ParticleModule::ToJson
		nlohmann::json ToJson() const override;
		/// @copydoc ParticleModule::FromJson
		void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI
		/// @copydoc ParticleModule::DrawInspector
		void DrawInspector() override;
#endif

		/// @brief 検証済みの角速度範囲を粒子の初期値生成へ一括適用する
		void ConfigureAngularVelocity(const RandomVector3& range) { angularVelocity_ = range; }
	private:
		RandomVector3 angularVelocity_;
	};
}
