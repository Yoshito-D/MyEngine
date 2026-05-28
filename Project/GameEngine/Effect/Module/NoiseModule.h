#pragma once
#include "ParticleModule.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
	// ============================================================
	// Noise Module
	// ============================================================
	class NoiseModule : public ParticleModule {
	public:
		NoiseModule();

		/// @brief パーティクルにノイズを適用
		void ApplyNoise(Particle& particle, float deltaTime);

		void SetStrength(float strength) { strength_ = strength; }
		float GetStrength() const { return strength_; }

		void SetFrequency(float frequency) { frequency_ = frequency; }
		float GetFrequency() const { return frequency_; }

		void SetScrollSpeed(float speed) { scrollSpeed_ = speed; }
		float GetScrollSpeed() const { return scrollSpeed_; }

		nlohmann::json ToJson() const override;
		void FromJson(const nlohmann::json& json) override;

	private:
		float strength_ = 1.0f;
		float frequency_ = 0.5f;
		float scrollSpeed_ = 1.0f;
		float noiseTime_ = 0.0f;
	};
}
