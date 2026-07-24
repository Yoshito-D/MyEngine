#include "pch.h"
#include "ForceOverLifetimeModule.h"
#include <cmath>

namespace GameEngine {
	ForceOverLifetimeModule::ForceOverLifetimeModule() = default;

	void ForceOverLifetimeModule::InitializeParticle(Particle& particle) const {
		particle.forceOverLifetimeForce = force_.GetValue();
		particle.drag = std::max(drag_.GetValue(), 0.0f);
	}

	void ForceOverLifetimeModule::ApplyForce(Particle& particle) const {
		ApplyForce(particle, 0.0f, Transform{}, false);
	}

	void ForceOverLifetimeModule::ApplyForce(Particle& particle, const Transform& simulationTransform, bool useLocalSimulation) const {
		ApplyForce(particle, 0.0f, simulationTransform, useLocalSimulation);
	}

	void ForceOverLifetimeModule::ApplyForce(Particle& particle, float deltaTime, const Transform& simulationTransform, bool useLocalSimulation) const {
		if (!enabled_) return;
		Vector3 force = force_.randomize ? particle.forceOverLifetimeForce : force_.minValue;
		if (useLocalSimulation) {
			force = RotateVector(force, simulationTransform.GetActiveQuaternion());
		}
		particle.acceleration += force;

		if (attractorEnabled_ && attractorStrength_ != 0.0f) {
			Vector3 target = attractorPosition_;
			if (useLocalSimulation) {
				target = simulationTransform.translation +
					RotateVector(attractorPosition_, simulationTransform.GetActiveQuaternion());
			}

			Vector3 toTarget = target - particle.transform.translation;
			const float distance = toTarget.Length();
			if (distance > 0.0001f && (attractorRadius_ <= 0.0f || distance <= attractorRadius_)) {
				float attenuation = 1.0f;
				if (attractorFalloff_ > 0.0f) {
					attenuation = attractorRadius_ > 0.0f
						? std::pow(std::max(1.0f - distance / attractorRadius_, 0.0f), attractorFalloff_)
						: 1.0f / std::pow(std::max(distance, 1.0f), attractorFalloff_);
				}
				particle.acceleration += (toTarget / distance) * (attractorStrength_ * attenuation);
			}
		}

		// 指数減衰によりフレームレートに依存しない空気抵抗にする。
		const float drag = drag_.randomize ? particle.drag : drag_.minValue;
		if (drag > 0.0f && deltaTime > 0.0f) {
			particle.velocity = particle.velocity * std::exp(-drag * deltaTime);
		}
	}

	nlohmann::json ForceOverLifetimeModule::ToJson() const {
		nlohmann::json j;
		j["enabled"] = enabled_;
		j["force"] = force_.ToJson();
		j["drag"] = drag_.ToJson();
		j["attractorEnabled"] = attractorEnabled_;
		j["attractorPosition"] = { attractorPosition_.x, attractorPosition_.y, attractorPosition_.z };
		j["attractorStrength"] = attractorStrength_;
		j["attractorRadius"] = attractorRadius_;
		j["attractorFalloff"] = attractorFalloff_;
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
		if (j.contains("drag")) {
			if (j["drag"].is_object()) drag_.FromJson(j["drag"]);
			else SetDrag(j["drag"]);
		}
		if (j.contains("attractorEnabled")) attractorEnabled_ = j["attractorEnabled"];
		if (j.contains("attractorPosition") && j["attractorPosition"].is_array() && j["attractorPosition"].size() >= 3) {
			attractorPosition_ = Vector3(j["attractorPosition"][0], j["attractorPosition"][1], j["attractorPosition"][2]);
		}
		if (j.contains("attractorStrength")) attractorStrength_ = j["attractorStrength"];
		if (j.contains("attractorRadius")) SetAttractorRadius(j["attractorRadius"]);
		if (j.contains("attractorFalloff")) SetAttractorFalloff(j["attractorFalloff"]);
	}
}
