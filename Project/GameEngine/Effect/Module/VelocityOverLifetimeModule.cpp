#include "pch.h"
#include "VelocityOverLifetimeModule.h"

namespace GameEngine {
	VelocityOverLifetimeModule::VelocityOverLifetimeModule() = default;

	void VelocityOverLifetimeModule::ApplyVelocity(Particle& particle, float deltaTime) const {
		if (!enabled_) return;
		particle.velocity += linearVelocity_ * deltaTime;
		if (speedModifier_ != 1.0f) {
			particle.velocity = particle.velocity * speedModifier_;
		}
	}

	nlohmann::json VelocityOverLifetimeModule::ToJson() const {
		nlohmann::json j;
		j["enabled"] = enabled_;
		j["linearVelocity"] = {linearVelocity_.x, linearVelocity_.y, linearVelocity_.z};
		j["speedModifier"] = speedModifier_;
		return j;
	}

	void VelocityOverLifetimeModule::FromJson(const nlohmann::json& j) {
		if (j.contains("enabled")) enabled_ = j["enabled"];
		if (j.contains("linearVelocity")) {
			auto arr = j["linearVelocity"];
			linearVelocity_ = Vector3{arr[0], arr[1], arr[2]};
		}
		if (j.contains("speedModifier")) speedModifier_ = j["speedModifier"];
	}
}
