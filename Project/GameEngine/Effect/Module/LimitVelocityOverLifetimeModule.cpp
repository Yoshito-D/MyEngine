#include "pch.h"
#include "LimitVelocityOverLifetimeModule.h"

namespace GameEngine {
	LimitVelocityOverLifetimeModule::LimitVelocityOverLifetimeModule() = default;

	void LimitVelocityOverLifetimeModule::InitializeParticle(Particle& particle) const {
		particle.limitVelocitySpeedLimit = speedLimit_.GetValue();
		particle.limitVelocityDampen = dampen_.GetValue();
	}

	void LimitVelocityOverLifetimeModule::LimitVelocity(Particle& particle) const {
		LimitVelocity(particle, Transform{}, false);
	}

	void LimitVelocityOverLifetimeModule::LimitVelocity(Particle& particle, const Transform&, bool) const {
		if (!enabled_) return;
		float speed = particle.velocity.Length();
		float speedLimit = speedLimit_.randomize ? particle.limitVelocitySpeedLimit : speedLimit_.minValue;
		float dampen = dampen_.randomize ? particle.limitVelocityDampen : dampen_.minValue;
		if (speed > speedLimit) {
			float excess = speed - speedLimit;
			particle.velocity = particle.velocity * (1.0f - dampen * excess / speed);
		}
	}

	nlohmann::json LimitVelocityOverLifetimeModule::ToJson() const {
		nlohmann::json j;
		j["enabled"] = enabled_;
		j["speedLimit"] = speedLimit_.ToJson();
		j["dampen"] = dampen_.ToJson();
		return j;
	}

	void LimitVelocityOverLifetimeModule::FromJson(const nlohmann::json& j) {
		if (j.contains("enabled")) enabled_ = j["enabled"];
		if (j.contains("speedLimit")) {
			if (j["speedLimit"].is_object()) {
				speedLimit_.FromJson(j["speedLimit"]);
			} else {
				float value = j["speedLimit"];
				speedLimit_ = RandomFloat(value, value, false);
			}
		}
		if (j.contains("dampen")) {
			if (j["dampen"].is_object()) {
				dampen_.FromJson(j["dampen"]);
			} else {
				float value = j["dampen"];
				dampen_ = RandomFloat(value, value, false);
			}
		}
	}
}
