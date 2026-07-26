#include "pch.h"
#include "NoiseModule.h"
#include <cmath>

namespace GameEngine {
	NoiseModule::NoiseModule() = default;

	void NoiseModule::InitializeParticle(Particle& particle) const {
		particle.noiseStrength = strength_.GetValue();
		particle.noiseFrequency = frequency_.GetValue();
		particle.noiseScrollSpeed = scrollSpeed_.GetValue();
		particle.noiseTime = 0.0f;
	}

	void NoiseModule::ApplyNoise(Particle& particle, float deltaTime) {
		ApplyNoise(particle, deltaTime, Transform{}, false);
	}

	void NoiseModule::ApplyNoise(Particle& particle, float deltaTime, const Transform& simulationTransform, bool useLocalSimulation) {
		if (!enabled_) return;
		float strength = strength_.randomize ? particle.noiseStrength : strength_.minValue;
		float frequency = frequency_.randomize ? particle.noiseFrequency : frequency_.minValue;
		float scrollSpeed = scrollSpeed_.randomize ? particle.noiseScrollSpeed : scrollSpeed_.minValue;

		particle.noiseTime += scrollSpeed * deltaTime;

		Vector3 samplePosition = particle.transform.translation;
		Quaternion simulationRotation = simulationTransform.GetActiveQuaternion();
		if (useLocalSimulation) {
			// エミッターと一緒にノイズ場を移動させるため、サンプル位置をローカル空間へ戻す。
			samplePosition = RotateVector(samplePosition - simulationTransform.translation, simulationRotation.Inverse());
		}

		// 軸ごとに位相速度を変え、三軸が同時に反転する規則的な揺れを避ける。
		Vector3 noiseVelocity{
			std::sin(samplePosition.x * frequency + particle.noiseTime) * strength,
			std::sin(samplePosition.y * frequency + particle.noiseTime * 1.3f) * strength,
			std::sin(samplePosition.z * frequency + particle.noiseTime * 0.7f) * strength
		};

		if (useLocalSimulation) {
			// ローカルで生成した揺れを、ワールド空間の粒子速度へ合わせて回転する。
			noiseVelocity = RotateVector(noiseVelocity, simulationRotation);
		}

		particle.velocity += noiseVelocity * deltaTime;
	}

	nlohmann::json NoiseModule::ToJson() const {
		nlohmann::json j;
		j["enabled"] = enabled_;
		j["strength"] = strength_.ToJson();
		j["frequency"] = frequency_.ToJson();
		j["scrollSpeed"] = scrollSpeed_.ToJson();
		return j;
	}

	void NoiseModule::FromJson(const nlohmann::json& j) {
		if (j.contains("enabled")) enabled_ = j["enabled"];
		if (j.contains("strength")) {
			if (j["strength"].is_object()) {
				strength_.FromJson(j["strength"]);
			} else {
				// 旧JSONのスカラー値はランダム化しない固定範囲として移行する。
				float value = j["strength"];
				strength_ = RandomFloat(value, value, false);
			}
		}
		if (j.contains("frequency")) {
			if (j["frequency"].is_object()) {
				frequency_.FromJson(j["frequency"]);
			} else {
				float value = j["frequency"];
				frequency_ = RandomFloat(value, value, false);
			}
		}
		if (j.contains("scrollSpeed")) {
			if (j["scrollSpeed"].is_object()) {
				scrollSpeed_.FromJson(j["scrollSpeed"]);
			} else {
				float value = j["scrollSpeed"];
				scrollSpeed_ = RandomFloat(value, value, false);
			}
		}
	}
}
