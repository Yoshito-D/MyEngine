#include "pch.h"
#include "ForceOverLifetimeModule.h"

namespace GameEngine {
	ForceOverLifetimeModule::ForceOverLifetimeModule() = default;

	void ForceOverLifetimeModule::ApplyForce(Particle& particle) const {
		if (!enabled_) return;
		particle.acceleration += force_;
	}

	nlohmann::json ForceOverLifetimeModule::ToJson() const {
		nlohmann::json j;
		j["enabled"] = enabled_;
		j["force"] = {force_.x, force_.y, force_.z};
		return j;
	}

	void ForceOverLifetimeModule::FromJson(const nlohmann::json& j) {
		if (j.contains("enabled")) enabled_ = j["enabled"];
		if (j.contains("force")) {
			auto arr = j["force"];
			force_ = Vector3{arr[0], arr[1], arr[2]};
		}
	}
}
