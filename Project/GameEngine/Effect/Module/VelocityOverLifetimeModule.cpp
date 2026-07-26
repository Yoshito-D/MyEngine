#include "pch.h"
#include "VelocityOverLifetimeModule.h"

namespace GameEngine {
	VelocityOverLifetimeModule::VelocityOverLifetimeModule() = default;

	void VelocityOverLifetimeModule::InitializeParticle(Particle& particle) const {
		particle.velocityOverLifetimeLinearVelocity = linearVelocity_.GetValue();
		particle.velocityOverLifetimeSpeedModifier = speedModifier_.GetValue();
	}

	void VelocityOverLifetimeModule::ApplyVelocity(Particle& particle, float deltaTime) const {
		ApplyVelocity(particle, deltaTime, Transform{}, false);
	}

	void VelocityOverLifetimeModule::ApplyVelocity(Particle& particle, float deltaTime, const Transform& simulationTransform, bool useLocalSimulation) const {
		if (!enabled_) return;
		Vector3 linearVelocity = linearVelocity_.randomize ? particle.velocityOverLifetimeLinearVelocity : linearVelocity_.minValue;
		if (useLocalSimulation) {
			// 設定値はエミッターのローカル方向として解釈し、加算前にワールド方向へ回転する。
			linearVelocity = RotateVector(linearVelocity, simulationTransform.GetActiveQuaternion());
		}

		particle.velocity += linearVelocity * deltaTime;

		float speedModifier = speedModifier_.randomize ? particle.velocityOverLifetimeSpeedModifier : speedModifier_.minValue;
		if (speedModifier != 1.0f) {
			particle.velocity = particle.velocity * speedModifier;
		}
	}

	nlohmann::json VelocityOverLifetimeModule::ToJson() const {
		nlohmann::json j;
		j["enabled"] = enabled_;
		j["linearVelocity"] = linearVelocity_.ToJson();
		j["speedModifier"] = speedModifier_.ToJson();
		return j;
	}

	void VelocityOverLifetimeModule::FromJson(const nlohmann::json& j) {
		if (j.contains("enabled")) enabled_ = j["enabled"];
		if (j.contains("linearVelocity")) {
			if (j["linearVelocity"].is_object()) {
				linearVelocity_.FromJson(j["linearVelocity"]);
			} else if (j["linearVelocity"].is_array()) {
				// 旧JSONのVector3は固定範囲へ変換し、見た目を変えずに新形式へ移行する。
				auto arr = j["linearVelocity"];
				Vector3 value{arr[0], arr[1], arr[2]};
				linearVelocity_ = RandomVector3(value, value, false);
			}
		}
		if (j.contains("speedModifier")) {
			if (j["speedModifier"].is_object()) {
				speedModifier_.FromJson(j["speedModifier"]);
			} else {
				float value = j["speedModifier"];
				speedModifier_ = RandomFloat(value, value, false);
			}
		}
	}
}
