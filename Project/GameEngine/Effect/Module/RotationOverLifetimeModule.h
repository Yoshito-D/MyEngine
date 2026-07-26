#pragma once
#include "ParticleModule.h"
#include "Utility/VectorMath.h"
#include "Utility/MathUtils.h"
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

		/// @brief 角速度の乱数範囲下限を設定する
		void SetAngularVelocityMin(const Vector3& minVal) { angularVelocityMin_ = minVal; }
		/// @brief 角速度の乱数範囲下限を取得する
		const Vector3& GetAngularVelocityMin() const { return angularVelocityMin_; }

		/// @brief 角速度の乱数範囲上限を設定する
		void SetAngularVelocityMax(const Vector3& maxVal) { angularVelocityMax_ = maxVal; }
		/// @brief 角速度の乱数範囲上限を取得する
		const Vector3& GetAngularVelocityMax() const { return angularVelocityMax_; }

		/// @brief 粒子ごとに角速度をランダム化するか設定する
		void SetAngularVelocityRandomize(bool randomize) { angularVelocityRandomize_ = randomize; }
		/// @brief 角速度のランダム化が有効か取得する
		bool GetAngularVelocityRandomize() const { return angularVelocityRandomize_; }

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

	private:
		Vector3 angularVelocityMin_{0.0f, 0.0f, 0.0f};
		Vector3 angularVelocityMax_{0.0f, 0.0f, 0.0f};
		bool angularVelocityRandomize_ = false;
	};
}
