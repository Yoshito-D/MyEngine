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
		/// @brief 生存期間カラー設定を既定値で初期化する
		ColorOverLifetimeModule();

		/// @brief パーティクルの色を更新
		void UpdateColor(Particle& particle) const;

		/// @brief 生存期間の開始色を設定する
		void SetStartColor(const Vector4& color) { startColor_ = color; }
		/// @brief 生存期間の開始色を取得する
		const Vector4& GetStartColor() const { return startColor_; }

		/// @brief 生存期間の終了色を設定する
		void SetEndColor(const Vector4& color) { endColor_ = color; }
		/// @brief 生存期間の終了色を取得する
		const Vector4& GetEndColor() const { return endColor_; }

		/// @copydoc ParticleModule::ToJson
		nlohmann::json ToJson() const override;
		/// @copydoc ParticleModule::FromJson
		void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI
		/// @copydoc ParticleModule::DrawInspector
		void DrawInspector() override;
#endif

	private:
		Vector4 startColor_{1.0f, 1.0f, 1.0f, 1.0f};
		Vector4 endColor_{1.0f, 1.0f, 1.0f, 0.0f};
	};
}
