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
		NoiseModule();

		/// @brief ランダム値を粒子ごとに初期化
		void InitializeParticle(Particle& particle) const;

		/// @brief パーティクルにノイズを適用
		void ApplyNoise(Particle& particle, float deltaTime);
		void ApplyNoise(Particle& particle, float deltaTime, const Transform& simulationTransform, bool useLocalSimulation);

		void SetStrength(float strength) { strength_ = RandomFloat(strength, strength, false); }
		float GetStrength() const { return strength_.minValue; }
		void SetStrengthRange(const RandomFloat& strength) { strength_ = strength; }
		const RandomFloat& GetStrengthRange() const { return strength_; }

		void SetFrequency(float frequency) { frequency_ = RandomFloat(frequency, frequency, false); }
		float GetFrequency() const { return frequency_.minValue; }
		void SetFrequencyRange(const RandomFloat& frequency) { frequency_ = frequency; }
		const RandomFloat& GetFrequencyRange() const { return frequency_; }

		void SetScrollSpeed(float speed) { scrollSpeed_ = RandomFloat(speed, speed, false); }
		float GetScrollSpeed() const { return scrollSpeed_.minValue; }
		void SetScrollSpeedRange(const RandomFloat& speed) { scrollSpeed_ = speed; }
		const RandomFloat& GetScrollSpeedRange() const { return scrollSpeed_; }

		nlohmann::json ToJson() const override;
		void FromJson(const nlohmann::json& json) override;

	private:
		RandomFloat strength_{1.0f, 1.0f, false};
		RandomFloat frequency_{0.5f, 0.5f, false};
		RandomFloat scrollSpeed_{1.0f, 1.0f, false};
	};
}
