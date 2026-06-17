#include "pch.h"
#include "ForceOverLifetimeModule.h"

namespace GameEngine {
	ForceOverLifetimeModule::ForceOverLifetimeModule() = default;

	void ForceOverLifetimeModule::InitializeParticle(Particle& particle) const {
		particle.forceOverLifetimeForce = force_.GetValue();
	}

	void ForceOverLifetimeModule::ApplyForce(Particle& particle) const {
		ApplyForce(particle, Transform{}, false);
	}

	void ForceOverLifetimeModule::ApplyForce(Particle& particle, const Transform& simulationTransform, bool useLocalSimulation) const {
		if (!enabled_) return;
		Vector3 force = force_.randomize ? particle.forceOverLifetimeForce : force_.minValue;
		if (useLocalSimulation) {
			force = RotateVector(force, simulationTransform.GetActiveQuaternion());
		}
		particle.acceleration += force;
	}

	nlohmann::json ForceOverLifetimeModule::ToJson() const {
		nlohmann::json j;
		j["enabled"] = enabled_;
		j["force"] = force_.ToJson();
		return j;
	}

	void ForceOverLifetimeModule::FromJson(const nlohmann::json& j) {
		if (j.contains("enabled")) enabled_ = j["enabled"];
		if (j.contains("force")) {
			if (j["force"].is_object()) {
				force_.FromJson(j["force"]);
			} else if (j["force"].is_array()) {
				auto arr = j["force"];
				Vector3 value{arr[0], arr[1], arr[2]};
				force_ = RandomVector3(value, value, false);
			}
		}
	}
}
