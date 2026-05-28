#include "pch.h"
#include "LimitVelocityOverLifetimeModule.h"

namespace GameEngine {
	LimitVelocityOverLifetimeModule::LimitVelocityOverLifetimeModule() = default;

	void LimitVelocityOverLifetimeModule::LimitVelocity(Particle& particle) const {
		if (!enabled_) return;
		float speed = particle.velocity.Length();
		if (speed > speedLimit_) {
			float excess = speed - speedLimit_;
			particle.velocity = particle.velocity * (1.0f - dampen_ * excess / speed);
		}
	}

	nlohmann::json LimitVelocityOverLifetimeModule::ToJson() const {
		nlohmann::json j;
		j["enabled"] = enabled_;
		j["speedLimit"] = speedLimit_;
		j["dampen"] = dampen_;
		return j;
	}

	void LimitVelocityOverLifetimeModule::FromJson(const nlohmann::json& j) {
		if (j.contains("enabled")) enabled_ = j["enabled"];
		if (j.contains("speedLimit")) speedLimit_ = j["speedLimit"];
		if (j.contains("dampen")) dampen_ = j["dampen"];
	}
}
