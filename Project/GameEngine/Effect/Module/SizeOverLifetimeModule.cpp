#include "pch.h"
#include "SizeOverLifetimeModule.h"

namespace GameEngine {
	SizeOverLifetimeModule::SizeOverLifetimeModule() = default;

	void SizeOverLifetimeModule::InitializeParticle(Particle& particle) const {
		particle.sizeOverLifetimeMultiplier = sizeMultiplier_.GetValue();
	}

	void SizeOverLifetimeModule::UpdateSize(Particle& particle) const {
		if (!enabled_) return;
		float t = particle.GetLifeProgress();
		const float multiplier = sizeMultiplier_.randomize ? particle.sizeOverLifetimeMultiplier : sizeMultiplier_.minValue;
		Vector3 size = (startSize_ + (endSize_ - startSize_) * t) * multiplier;
		particle.currentSize = size;
		particle.transform.scale = size;
	}

	nlohmann::json SizeOverLifetimeModule::ToJson() const {
		nlohmann::json j;
		j["enabled"] = enabled_;
		j["sizeMultiplier"] = sizeMultiplier_.ToJson();
		j["startSize"] = {startSize_.x, startSize_.y, startSize_.z};
		j["endSize"] = {endSize_.x, endSize_.y, endSize_.z};
		return j;
	}

	void SizeOverLifetimeModule::FromJson(const nlohmann::json& j) {
		if (j.contains("enabled")) enabled_ = j["enabled"];
		if (j.contains("sizeMultiplier")) {
			if (j["sizeMultiplier"].is_object()) sizeMultiplier_.FromJson(j["sizeMultiplier"]);
			else SetSizeMultiplier(j["sizeMultiplier"]);
		}
		if (j.contains("startSize")) {
			if (j["startSize"].is_array()) {
				auto arr = j["startSize"];
				startSize_ = Vector3{arr[0], arr[1], arr[2]};
			} else {
				float value = j["startSize"];
				startSize_ = Vector3(value, value, value);
			}
		}
		if (j.contains("endSize")) {
			if (j["endSize"].is_array()) {
				auto arr = j["endSize"];
				endSize_ = Vector3{arr[0], arr[1], arr[2]};
			} else {
				float value = j["endSize"];
				endSize_ = Vector3(value, value, value);
			}
		}
	}
}
