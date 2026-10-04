#include "GameEngine/pch.h"
#include "GameEngine/Effects/Particles/Modules/RotationOverLifetimeModule.h"

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
      return angularVelocity_.GetValue();
   }

	nlohmann::json RotationOverLifetimeModule::ToJson() const {
		nlohmann::json j;
		j["enabled"] = enabled_;
		j["angularVelocityMin"] = {angularVelocity_.Minimum().x, angularVelocity_.Minimum().y, angularVelocity_.Minimum().z};
		j["angularVelocityMax"] = {angularVelocity_.Maximum().x, angularVelocity_.Maximum().y, angularVelocity_.Maximum().z};
		j["angularVelocityRandomize"] = angularVelocity_.IsRandomized();
		return j;
	}

   void RotationOverLifetimeModule::FromJson(const nlohmann::json& j) {
      if (j.contains("enabled")) enabled_ = j["enabled"];
      auto low = angularVelocity_.Minimum(); auto high = angularVelocity_.Maximum();
      bool randomized = angularVelocity_.IsRandomized();
      auto readVector = [&](const char* name, Vector3& value) {
         if (j.contains(name) && j[name].is_array() && j[name].size() >= 3)
            value = Vector3{ j[name][0], j[name][1], j[name][2] };
      };
      if (j.contains("angularVelocity")) { readVector("angularVelocity", low); high = low; randomized = false; }
      readVector("angularVelocityMin", low); readVector("angularVelocityMax", high);
      randomized = j.value("angularVelocityRandomize", randomized);
      ConfigureAngularVelocity(RandomVector3(low, high, randomized));
   }
}
