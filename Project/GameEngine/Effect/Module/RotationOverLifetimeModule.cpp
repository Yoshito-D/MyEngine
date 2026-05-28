#include "pch.h"
#include "RotationOverLifetimeModule.h"

namespace GameEngine {
	RotationOverLifetimeModule::RotationOverLifetimeModule() = default;

	void RotationOverLifetimeModule::UpdateRotation(Particle& particle, float deltaTime) const {
		particle.transform.rotation.x += particle.angularVelocity.x * deltaTime;
		particle.transform.rotation.y += particle.angularVelocity.y * deltaTime;
		particle.transform.rotation.z += particle.angularVelocity.z * deltaTime;
	}

	Vector3 RotationOverLifetimeModule::GetRandomAngularVelocity() const {
		if (!angularVelocityRandomize_) return angularVelocityMin_;
		return Vector3(
			RandomUtils::Random(angularVelocityMin_.x, angularVelocityMax_.x),
			RandomUtils::Random(angularVelocityMin_.y, angularVelocityMax_.y),
			RandomUtils::Random(angularVelocityMin_.z, angularVelocityMax_.z)
		);
	}

	nlohmann::json RotationOverLifetimeModule::ToJson() const {
		nlohmann::json j;
		j["enabled"] = enabled_;
		j["angularVelocityMin"] = {angularVelocityMin_.x, angularVelocityMin_.y, angularVelocityMin_.z};
		j["angularVelocityMax"] = {angularVelocityMax_.x, angularVelocityMax_.y, angularVelocityMax_.z};
		j["angularVelocityRandomize"] = angularVelocityRandomize_;
		return j;
	}

	void RotationOverLifetimeModule::FromJson(const nlohmann::json& j) {
		if (j.contains("enabled")) enabled_ = j["enabled"];
		if (j.contains("angularVelocity") && j["angularVelocity"].is_array()) {
			auto arr = j["angularVelocity"];
			angularVelocityMin_ = Vector3{arr[0], arr[1], arr[2]};
			angularVelocityMax_ = angularVelocityMin_;
			angularVelocityRandomize_ = false;
		}
		if (j.contains("angularVelocityMin") && j["angularVelocityMin"].is_array()) {
			auto arr = j["angularVelocityMin"];
			angularVelocityMin_ = Vector3{arr[0], arr[1], arr[2]};
		}
		if (j.contains("angularVelocityMax") && j["angularVelocityMax"].is_array()) {
			auto arr = j["angularVelocityMax"];
			angularVelocityMax_ = Vector3{arr[0], arr[1], arr[2]};
		}
		if (j.contains("angularVelocityRandomize")) {
			angularVelocityRandomize_ = j["angularVelocityRandomize"];
		}
	}
}
