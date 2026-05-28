#include "pch.h"
#include "NoiseModule.h"
#include <cmath>

namespace GameEngine {
	NoiseModule::NoiseModule() = default;

	void NoiseModule::ApplyNoise(Particle& particle, float deltaTime) {
		if (!enabled_) return;
		noiseTime_ += scrollSpeed_ * deltaTime;
		float noiseX = std::sin(particle.transform.translation.x * frequency_ + noiseTime_) * strength_;
		float noiseY = std::sin(particle.transform.translation.y * frequency_ + noiseTime_ * 1.3f) * strength_;
		float noiseZ = std::sin(particle.transform.translation.z * frequency_ + noiseTime_ * 0.7f) * strength_;
		particle.velocity.x += noiseX * deltaTime;
		particle.velocity.y += noiseY * deltaTime;
		particle.velocity.z += noiseZ * deltaTime;
	}

	nlohmann::json NoiseModule::ToJson() const {
		nlohmann::json j;
		j["enabled"] = enabled_;
		j["strength"] = strength_;
		j["frequency"] = frequency_;
		j["scrollSpeed"] = scrollSpeed_;
		return j;
	}

	void NoiseModule::FromJson(const nlohmann::json& j) {
		if (j.contains("enabled")) enabled_ = j["enabled"];
		if (j.contains("strength")) strength_ = j["strength"];
		if (j.contains("frequency")) frequency_ = j["frequency"];
		if (j.contains("scrollSpeed")) scrollSpeed_ = j["scrollSpeed"];
	}
}
