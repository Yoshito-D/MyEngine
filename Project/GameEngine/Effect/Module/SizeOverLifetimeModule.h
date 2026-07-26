#pragma once
#include "ParticleModule.h"
#include "Utility/VectorMath.h"
#include "MainModule.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
	// ============================================================
	// Size over Lifetime Module
	// ============================================================
	class SizeOverLifetimeModule : public ParticleModule {
	public:
		/// @brief 生存期間サイズ設定を既定値で初期化する
		SizeOverLifetimeModule();

		/// @brief パーティクルのサイズを更新
		void UpdateSize(Particle& particle) const;

		/// @brief 粒子ごとのサイズ倍率を固定値で設定する
		void SetSizeMultiplier(float multiplier) { sizeMultiplier_ = RandomFloat(multiplier); }
		/// @brief サイズ倍率の代表値を取得する
		float GetSizeMultiplier() const { return sizeMultiplier_.minValue; }
		/// @brief 粒子ごとのサイズ倍率を乱数範囲で設定する
		void SetSizeMultiplierRange(const RandomFloat& multiplier) { sizeMultiplier_ = multiplier; }
		/// @brief サイズ倍率の乱数範囲を取得する
		const RandomFloat& GetSizeMultiplierRange() const { return sizeMultiplier_; }

		/// @brief 粒子固有のサイズ倍率を初期化する
		void InitializeParticle(Particle& particle) const;

		/// @brief 生存期間の開始時サイズを設定する
		void SetStartSize(const Vector3& size) { startSize_ = size; }
		/// @brief 生存期間の開始時サイズを取得する
		const Vector3& GetStartSize() const { return startSize_; }

		/// @brief 生存期間の終了時サイズを設定する
		void SetEndSize(const Vector3& size) { endSize_ = size; }
		/// @brief 生存期間の終了時サイズを取得する
		const Vector3& GetEndSize() const { return endSize_; }

		/// @copydoc ParticleModule::ToJson
		nlohmann::json ToJson() const override;
		/// @copydoc ParticleModule::FromJson
		void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI
		/// @copydoc ParticleModule::DrawInspector
		void DrawInspector() override;
#endif

	private:
		RandomFloat sizeMultiplier_{ 1.0f, 1.0f, false };
		Vector3 startSize_{1.0f, 1.0f, 1.0f};
		Vector3 endSize_{0.0f, 0.0f, 0.0f};
	};
}
