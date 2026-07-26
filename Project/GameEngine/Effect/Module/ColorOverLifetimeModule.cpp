#include "pch.h"
#include "ColorOverLifetimeModule.h"

namespace GameEngine {
	ColorOverLifetimeModule::ColorOverLifetimeModule() = default;

	void ColorOverLifetimeModule::UpdateColor(Particle& particle) const {
		if (!enabled_) return;
		float t = particle.GetLifeProgress();
		// RGBAを同じ寿命進行度で補間し、色相と透明度の変化を同期させる。
		particle.color.x = startColor_.x + (endColor_.x - startColor_.x) * t;
		particle.color.y = startColor_.y + (endColor_.y - startColor_.y) * t;
		particle.color.z = startColor_.z + (endColor_.z - startColor_.z) * t;
		particle.color.w = startColor_.w + (endColor_.w - startColor_.w) * t;
	}

	nlohmann::json ColorOverLifetimeModule::ToJson() const {
		nlohmann::json j;
		j["enabled"] = enabled_;
		j["startColor"] = {startColor_.x, startColor_.y, startColor_.z, startColor_.w};
		j["endColor"] = {endColor_.x, endColor_.y, endColor_.z, endColor_.w};
		return j;
	}

	void ColorOverLifetimeModule::FromJson(const nlohmann::json& j) {
		if (j.contains("enabled")) enabled_ = j["enabled"];
		if (j.contains("startColor")) {
			auto arr = j["startColor"];
			startColor_ = Vector4{arr[0], arr[1], arr[2], arr[3]};
		}
		if (j.contains("endColor")) {
			auto arr = j["endColor"];
			endColor_ = Vector4{arr[0], arr[1], arr[2], arr[3]};
		}
	}
}
