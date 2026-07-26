#include "pch.h"
#include "RotationOverLifetimeModule.h"

namespace GameEngine {
	RotationOverLifetimeModule::RotationOverLifetimeModule() = default;

	void RotationOverLifetimeModule::UpdateRotation(Particle& particle, float deltaTime) const {
		const Quaternion currentRotation = particle.transform.GetActiveQuaternion();
		const Vector3 deltaEuler = particle.angularVelocity * deltaTime;
		const Quaternion deltaRotation = Vector3ToQuaternion(deltaEuler);
		// 現在回転の右側へ増分を合成し、角速度をパーティクルのローカル軸として適用する。
		particle.transform.SetRotationQuaternion((currentRotation * deltaRotation).Normalize());
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
			// 旧JSONの固定角速度を上下限が同じ範囲へ移行する。
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
