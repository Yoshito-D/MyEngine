#pragma once
#include "ParticleModule.h"
#include "Utility/VectorMath.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
	// ============================================================
	// Color over Lifetime Module
	// ============================================================
	class ColorOverLifetimeModule : public ParticleModule {
	public:
		ColorOverLifetimeModule();

		/// @brief パーティクルの色を更新
		void UpdateColor(Particle& particle) const;

		void SetStartColor(const Vector4& color) { startColor_ = color; }
		const Vector4& GetStartColor() const { return startColor_; }

		void SetEndColor(const Vector4& color) { endColor_ = color; }
		const Vector4& GetEndColor() const { return endColor_; }

		nlohmann::json ToJson() const override;
		void FromJson(const nlohmann::json& json) override;

	private:
		Vector4 startColor_{1.0f, 1.0f, 1.0f, 1.0f};
		Vector4 endColor_{1.0f, 1.0f, 1.0f, 0.0f};
	};
}
