#pragma once
#include "ParticleModule.h"
#include "Utility/VectorMath.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
	// ============================================================
	// Size over Lifetime Module
	// ============================================================
	class SizeOverLifetimeModule : public ParticleModule {
	public:
		SizeOverLifetimeModule();

		/// @brief パーティクルのサイズを更新
		void UpdateSize(Particle& particle) const;

		void SetSizeMultiplier(float multiplier) { sizeMultiplier_ = multiplier; }
		float GetSizeMultiplier() const { return sizeMultiplier_; }

		void SetStartSize(const Vector3& size) { startSize_ = size; }
		const Vector3& GetStartSize() const { return startSize_; }

		void SetEndSize(const Vector3& size) { endSize_ = size; }
		const Vector3& GetEndSize() const { return endSize_; }

		nlohmann::json ToJson() const override;
		void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI
		void DrawInspector() override;
#endif

	private:
		float sizeMultiplier_ = 1.0f;
		Vector3 startSize_{1.0f, 1.0f, 1.0f};
		Vector3 endSize_{0.0f, 0.0f, 0.0f};
	};
}
