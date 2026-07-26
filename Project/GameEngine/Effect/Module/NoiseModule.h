#pragma once
#include "ParticleModule.h"
#include "MainModule.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
	// ============================================================
	// Noise Module
	// ============================================================
	class NoiseModule : public ParticleModule {
	public:
		/// @brief ノイズ設定を既定値で初期化する
		NoiseModule();

		/// @brief ランダム値を粒子ごとに初期化
		void InitializeParticle(Particle& particle) const;

		/// @brief パーティクルにノイズを適用
		void ApplyNoise(Particle& particle, float deltaTime);
		/// @brief シミュレーション空間を考慮してノイズ変位を適用する
		void ApplyNoise(Particle& particle, float deltaTime, const Transform& simulationTransform, bool useLocalSimulation);

		/// @brief ノイズ強度を固定値で設定する
		void SetStrength(float strength) { strength_ = RandomFloat(strength, strength, false); }
		/// @brief ノイズ強度の代表値を取得する
		float GetStrength() const { return strength_.minValue; }
		/// @brief ノイズ強度を乱数範囲で設定する
		void SetStrengthRange(const RandomFloat& strength) { strength_ = strength; }
		/// @brief ノイズ強度の乱数範囲を取得する
		const RandomFloat& GetStrengthRange() const { return strength_; }

		/// @brief ノイズ周波数を固定値で設定する
		void SetFrequency(float frequency) { frequency_ = RandomFloat(frequency, frequency, false); }
		/// @brief ノイズ周波数の代表値を取得する
		float GetFrequency() const { return frequency_.minValue; }
		/// @brief ノイズ周波数を乱数範囲で設定する
		void SetFrequencyRange(const RandomFloat& frequency) { frequency_ = frequency; }
		/// @brief ノイズ周波数の乱数範囲を取得する
		const RandomFloat& GetFrequencyRange() const { return frequency_; }

		/// @brief ノイズ座標のスクロール速度を固定値で設定する
		void SetScrollSpeed(float speed) { scrollSpeed_ = RandomFloat(speed, speed, false); }
		/// @brief ノイズ座標のスクロール速度の代表値を取得する
		float GetScrollSpeed() const { return scrollSpeed_.minValue; }
		/// @brief ノイズ座標のスクロール速度を乱数範囲で設定する
		void SetScrollSpeedRange(const RandomFloat& speed) { scrollSpeed_ = speed; }
		/// @brief ノイズ座標のスクロール速度の乱数範囲を取得する
		const RandomFloat& GetScrollSpeedRange() const { return scrollSpeed_; }

		/// @copydoc ParticleModule::ToJson
		nlohmann::json ToJson() const override;
		/// @copydoc ParticleModule::FromJson
		void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI
		/// @copydoc ParticleModule::DrawInspector
		void DrawInspector() override;
#endif

	private:
		RandomFloat strength_{1.0f, 1.0f, false};
		RandomFloat frequency_{0.5f, 0.5f, false};
		RandomFloat scrollSpeed_{1.0f, 1.0f, false};
	};
}
