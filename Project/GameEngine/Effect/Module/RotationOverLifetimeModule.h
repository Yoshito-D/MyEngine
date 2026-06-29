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
		RotationOverLifetimeModule();

		/// @brief パーティクルの回転を更新
		void UpdateRotation(Particle& particle, float deltaTime) const;

		void SetAngularVelocityMin(const Vector3& minVal) { angularVelocityMin_ = minVal; }
		const Vector3& GetAngularVelocityMin() const { return angularVelocityMin_; }

		void SetAngularVelocityMax(const Vector3& maxVal) { angularVelocityMax_ = maxVal; }
		const Vector3& GetAngularVelocityMax() const { return angularVelocityMax_; }

		void SetAngularVelocityRandomize(bool randomize) { angularVelocityRandomize_ = randomize; }
		bool GetAngularVelocityRandomize() const { return angularVelocityRandomize_; }

		/// @brief ランダムな角速度を取得
		Vector3 GetRandomAngularVelocity() const;

		nlohmann::json ToJson() const override;
		void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI
		void DrawInspector() override;
#endif

	private:
		Vector3 angularVelocityMin_{0.0f, 0.0f, 0.0f};
		Vector3 angularVelocityMax_{0.0f, 0.0f, 0.0f};
		bool angularVelocityRandomize_ = false;
	};
}
