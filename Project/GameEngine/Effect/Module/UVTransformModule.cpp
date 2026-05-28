#include "pch.h"
#include "UVTransformModule.h"

namespace GameEngine {
UVTransformModule::UVTransformModule() = default;

Vector2 UVTransformModule::LerpVector2(const Vector2& a, const Vector2& b, float t) {
	return Vector2{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

float UVTransformModule::LerpFloat(float a, float b, float t) {
	return a + (b - a) * t;
}

void UVTransformModule::InitializeParticle(Particle& particle) const {
	if (!enabled_) return;

	switch (scrollMode_) {
	case ValueMode::Constant:
		particle.uvScroll = scrollConstant_;
		break;
	case ValueMode::RandomBetweenTwoConstants:
		particle.uvScroll = scrollRandom_.GetValue();
		break;
	case ValueMode::Curve:
		particle.uvScroll = scrollCurveStart_;
		break;
	}

	switch (rotationMode_) {
	case ValueMode::Constant:
		particle.uvRotation = rotationConstant_;
		break;
	case ValueMode::RandomBetweenTwoConstants:
		particle.uvRotation = rotationRandom_.GetValue();
		break;
	case ValueMode::Curve:
		particle.uvRotation = rotationCurveStart_;
		break;
	}

	switch (scaleMode_) {
	case ValueMode::Constant:
		particle.uvScale = scaleConstant_;
		break;
	case ValueMode::RandomBetweenTwoConstants:
		particle.uvScale = scaleRandom_.GetValue();
		break;
	case ValueMode::Curve:
		particle.uvScale = scaleCurveStart_;
		break;
	}

	particle.uvOffset = Vector2{0.0f, 0.0f};
}

void UVTransformModule::UpdateUV(Particle& particle, float deltaTime) const {
	if (!enabled_) return;

	const float t = particle.GetLifeProgress();
	Vector2 currentScroll = particle.uvScroll;

	switch (scrollMode_) {
	case ValueMode::Constant:
	case ValueMode::RandomBetweenTwoConstants:
		break;
	case ValueMode::Curve:
		currentScroll = LerpVector2(scrollCurveStart_, scrollCurveEnd_, t);
		particle.uvScroll = currentScroll;
		break;
	}

	particle.uvOffset += currentScroll * deltaTime;

	switch (rotationMode_) {
	case ValueMode::Constant:
	case ValueMode::RandomBetweenTwoConstants:
		break;
	case ValueMode::Curve:
		particle.uvRotation = LerpFloat(rotationCurveStart_, rotationCurveEnd_, t);
		break;
	}

	switch (scaleMode_) {
	case ValueMode::Constant:
	case ValueMode::RandomBetweenTwoConstants:
		break;
	case ValueMode::Curve:
		particle.uvScale = LerpVector2(scaleCurveStart_, scaleCurveEnd_, t);
		break;
	}
}

nlohmann::json UVTransformModule::ToJson() const {
	nlohmann::json j;
	j["enabled"] = enabled_;
	j["scrollMode"] = static_cast<int>(scrollMode_);
	j["scrollConstant"] = {scrollConstant_.x, scrollConstant_.y};
	j["scrollRandom"] = scrollRandom_.ToJson();
	j["scrollCurveStart"] = {scrollCurveStart_.x, scrollCurveStart_.y};
	j["scrollCurveEnd"] = {scrollCurveEnd_.x, scrollCurveEnd_.y};
	j["rotationMode"] = static_cast<int>(rotationMode_);
	j["rotationConstant"] = rotationConstant_;
	j["rotationRandom"] = rotationRandom_.ToJson();
	j["rotationCurveStart"] = rotationCurveStart_;
	j["rotationCurveEnd"] = rotationCurveEnd_;
	j["scaleMode"] = static_cast<int>(scaleMode_);
	j["scaleConstant"] = {scaleConstant_.x, scaleConstant_.y};
	j["scaleRandom"] = scaleRandom_.ToJson();
	j["scaleCurveStart"] = {scaleCurveStart_.x, scaleCurveStart_.y};
	j["scaleCurveEnd"] = {scaleCurveEnd_.x, scaleCurveEnd_.y};
	return j;
}

void UVTransformModule::FromJson(const nlohmann::json& j) {
	if (j.contains("enabled")) enabled_ = j["enabled"];
	if (j.contains("scrollMode")) scrollMode_ = static_cast<ValueMode>(j["scrollMode"].get<int>());
	if (j.contains("scrollConstant") && j["scrollConstant"].is_array() && j["scrollConstant"].size() >= 2) {
		auto arr = j["scrollConstant"];
		scrollConstant_ = Vector2{arr[0], arr[1]};
	}
	if (j.contains("scrollRandom") && j["scrollRandom"].is_object()) scrollRandom_.FromJson(j["scrollRandom"]);
	if (j.contains("scrollCurveStart") && j["scrollCurveStart"].is_array() && j["scrollCurveStart"].size() >= 2) {
		auto arr = j["scrollCurveStart"];
		scrollCurveStart_ = Vector2{arr[0], arr[1]};
	}
	if (j.contains("scrollCurveEnd") && j["scrollCurveEnd"].is_array() && j["scrollCurveEnd"].size() >= 2) {
		auto arr = j["scrollCurveEnd"];
		scrollCurveEnd_ = Vector2{arr[0], arr[1]};
	}
	if (j.contains("rotationMode")) rotationMode_ = static_cast<ValueMode>(j["rotationMode"].get<int>());
	if (j.contains("rotationConstant")) rotationConstant_ = j["rotationConstant"];
	if (j.contains("rotationRandom") && j["rotationRandom"].is_object()) rotationRandom_.FromJson(j["rotationRandom"]);
	if (j.contains("rotationCurveStart")) rotationCurveStart_ = j["rotationCurveStart"];
	if (j.contains("rotationCurveEnd")) rotationCurveEnd_ = j["rotationCurveEnd"];
	if (j.contains("scaleMode")) scaleMode_ = static_cast<ValueMode>(j["scaleMode"].get<int>());
	if (j.contains("scaleConstant") && j["scaleConstant"].is_array() && j["scaleConstant"].size() >= 2) {
		auto arr = j["scaleConstant"];
		scaleConstant_ = Vector2{arr[0], arr[1]};
	}
	if (j.contains("scaleRandom") && j["scaleRandom"].is_object()) scaleRandom_.FromJson(j["scaleRandom"]);
	if (j.contains("scaleCurveStart") && j["scaleCurveStart"].is_array() && j["scaleCurveStart"].size() >= 2) {
		auto arr = j["scaleCurveStart"];
		scaleCurveStart_ = Vector2{arr[0], arr[1]};
	}
	if (j.contains("scaleCurveEnd") && j["scaleCurveEnd"].is_array() && j["scaleCurveEnd"].size() >= 2) {
		auto arr = j["scaleCurveEnd"];
		scaleCurveEnd_ = Vector2{arr[0], arr[1]};
	}
}
}
