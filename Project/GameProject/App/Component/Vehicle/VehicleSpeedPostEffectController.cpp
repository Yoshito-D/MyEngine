#include "VehicleSpeedPostEffectController.h"
#include "VehicleGroundMover.h"
#include "Framework/EngineContext.h"
#include "Object/Object.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace {
float Saturate(float value) {
   return std::clamp(value, 0.0f, 1.0f);
}

float Lerp(float from, float to, float t) {
   return from + (to - from) * t;
}

float SmoothTowards(float current, float target, float responseSpeed, float deltaTime) {
   if (responseSpeed <= 0.0f || deltaTime <= 0.0f) {
	  return target;
   }

   const float alpha = 1.0f - std::exp(-responseSpeed * deltaTime);
   return Lerp(current, target, Saturate(alpha));
}
}

namespace App {

VehicleSpeedPostEffectController::~VehicleSpeedPostEffectController() {
   ApplyNeutralEffect();
}

void VehicleSpeedPostEffectController::Update(float deltaTime) {
   if (!HasOwner()) {
	  ApplyNeutralEffect();
	  return;
   }

   auto* groundMover = GetOwner().GetComponent<VehicleGroundMover>();
   if (!groundMover) {
	  ApplyNeutralEffect();
	  return;
   }

   time_ += std::max(deltaTime * 10.0f, 0.0f);

   const float autoSpeed = std::max(groundMover->autoSpeed, 0.01f);
   const float currentSpeed = std::max(groundMover->GetCurrentSpeed(), 0.0f);
   const float speedOverAuto = currentSpeed - autoSpeed - std::max(activationMargin, 0.0f);
   const float targetAmount = Saturate(speedOverAuto / std::max(effectSpeedRange, 0.01f));
   effectAmount_ = SmoothTowards(effectAmount_, targetAmount, responseSpeed, deltaTime);

   GameEngine::SpeedLineParams params{};
   params.center = { 0.5f, 0.5f };
   params.intensity = maxIntensity * effectAmount_;
   params.lineDensity = lineDensity;
   params.thickness = thickness;
   params.innerRadius = Lerp(idleInnerRadius, activeInnerRadius, effectAmount_);
   params.outerRadius = outerRadius;
   params.time = time_;
   params.randomSeed = randomSeed;
   params.flowSpeed = Lerp(idleFlowSpeed, activeFlowSpeed, effectAmount_);

   GameEngine::EngineContext::SetSpeedLineParams(params);
   GameEngine::EngineContext::SetPostProcessEffectEnabled("Speed Line", effectAmount_ >= std::max(visibleThreshold, 0.0f));

   GameEngine::RadialBlurParams radialParams{};
   radialParams.center = { 0.5f, 0.5f };
   radialParams.strength = radialBlurMaxStrength * effectAmount_;
   radialParams.sampleCount = radialBlurSampleCount;

   GameEngine::EngineContext::SetRadialBlurParams(radialParams);
   GameEngine::EngineContext::SetPostProcessEffectEnabled("Radial Blur", effectAmount_ >= std::max(radialBlurVisibleThreshold, 0.0f));
}

void VehicleSpeedPostEffectController::OnDetach() {
   ApplyNeutralEffect();
}

void VehicleSpeedPostEffectController::OnDisable() {
   ApplyNeutralEffect();
}

#ifdef USE_IMGUI
void VehicleSpeedPostEffectController::DrawInspector() {
   if (!ImGui::CollapsingHeader("VehicleSpeedPostEffectController")) { return; }

   ImGui::Separator();
   ImGui::DragFloat("Activation Margin", &activationMargin, 0.01f, 0.0f, 10.0f);
   ImGui::DragFloat("Effect Speed Range", &effectSpeedRange, 0.1f, 0.01f, 100.0f);
   ImGui::DragFloat("Response Speed", &responseSpeed, 0.1f, 0.0f, 30.0f);
   ImGui::DragFloat("Visible Threshold", &visibleThreshold, 0.001f, 0.0f, 1.0f);
   ImGui::DragFloat("Idle Inner Radius", &idleInnerRadius, 0.01f, 0.0f, 2.0f);
   ImGui::DragFloat("Active Inner Radius", &activeInnerRadius, 0.01f, 0.0f, 2.0f);
   ImGui::DragFloat("Outer Radius", &outerRadius, 0.01f, 0.0f, 2.0f);
   ImGui::DragFloat("Max Intensity", &maxIntensity, 0.01f, 0.0f, 3.0f);
   ImGui::DragFloat("Idle Flow Speed", &idleFlowSpeed, 0.01f, 0.0f, 5.0f);
   ImGui::DragFloat("Active Flow Speed", &activeFlowSpeed, 0.01f, 0.0f, 5.0f);
   ImGui::DragFloat("Line Density", &lineDensity, 1.0f, 16.0f, 512.0f);
   ImGui::DragFloat("Thickness", &thickness, 0.01f, 0.0f, 1.0f);
   ImGui::DragFloat("Random Seed", &randomSeed, 0.1f, 0.0f, 100.0f);
   ImGui::DragFloat("Radial Blur Max", &radialBlurMaxStrength, 0.001f, 0.0f, 0.1f, "%.3f");
   ImGui::SliderInt("Radial Samples", &radialBlurSampleCount, 2, 32);
   ImGui::DragFloat("Radial Visible Threshold", &radialBlurVisibleThreshold, 0.001f, 0.0f, 1.0f);
   ImGui::Text("EffectAmount: %.3f", effectAmount_);
}
#endif

nlohmann::json VehicleSpeedPostEffectController::Serialize() const {
   nlohmann::json json;
   json["activationMargin"] = activationMargin;
   json["effectSpeedRange"] = effectSpeedRange;
   json["responseSpeed"] = responseSpeed;
   json["visibleThreshold"] = visibleThreshold;
   json["idleInnerRadius"] = idleInnerRadius;
   json["activeInnerRadius"] = activeInnerRadius;
   json["outerRadius"] = outerRadius;
   json["maxIntensity"] = maxIntensity;
   json["idleFlowSpeed"] = idleFlowSpeed;
   json["activeFlowSpeed"] = activeFlowSpeed;
   json["lineDensity"] = lineDensity;
   json["thickness"] = thickness;
   json["randomSeed"] = randomSeed;
   json["radialBlurMaxStrength"] = radialBlurMaxStrength;
   json["radialBlurSampleCount"] = radialBlurSampleCount;
   json["radialBlurVisibleThreshold"] = radialBlurVisibleThreshold;
   return json;
}

void VehicleSpeedPostEffectController::Deserialize(const nlohmann::json& data) {
   if (data.contains("activationMargin")) { activationMargin = data["activationMargin"]; }
   if (data.contains("effectSpeedRange")) { effectSpeedRange = data["effectSpeedRange"]; }
   if (data.contains("responseSpeed")) { responseSpeed = data["responseSpeed"]; }
   if (data.contains("visibleThreshold")) { visibleThreshold = data["visibleThreshold"]; }
   if (data.contains("idleInnerRadius")) { idleInnerRadius = data["idleInnerRadius"]; }
   if (data.contains("activeInnerRadius")) { activeInnerRadius = data["activeInnerRadius"]; }
   if (data.contains("outerRadius")) { outerRadius = data["outerRadius"]; }
   if (data.contains("maxIntensity")) { maxIntensity = data["maxIntensity"]; }
   if (data.contains("idleFlowSpeed")) { idleFlowSpeed = data["idleFlowSpeed"]; }
   if (data.contains("activeFlowSpeed")) { activeFlowSpeed = data["activeFlowSpeed"]; }
   if (data.contains("lineDensity")) { lineDensity = data["lineDensity"]; }
   if (data.contains("thickness")) { thickness = data["thickness"]; }
   if (data.contains("randomSeed")) { randomSeed = data["randomSeed"]; }
   if (data.contains("radialBlurMaxStrength")) { radialBlurMaxStrength = data["radialBlurMaxStrength"]; }
   if (data.contains("radialBlurSampleCount")) { radialBlurSampleCount = data["radialBlurSampleCount"]; }
   if (data.contains("radialBlurVisibleThreshold")) { radialBlurVisibleThreshold = data["radialBlurVisibleThreshold"]; }
}

void VehicleSpeedPostEffectController::ApplyNeutralEffect() {
   effectAmount_ = 0.0f;

   GameEngine::SpeedLineParams params{};
   params.center = { 0.5f, 0.5f };
   params.intensity = 0.0f;
   params.lineDensity = lineDensity;
   params.thickness = thickness;
   params.innerRadius = idleInnerRadius;
   params.outerRadius = outerRadius;
   params.time = time_;
   params.randomSeed = randomSeed;
   params.flowSpeed = idleFlowSpeed;

   GameEngine::EngineContext::SetSpeedLineParams(params);
   GameEngine::EngineContext::SetPostProcessEffectEnabled("Speed Line", false);

   GameEngine::RadialBlurParams radialParams{};
   radialParams.center = { 0.5f, 0.5f };
   radialParams.strength = 0.0f;
   radialParams.sampleCount = radialBlurSampleCount;

   GameEngine::EngineContext::SetRadialBlurParams(radialParams);
   GameEngine::EngineContext::SetPostProcessEffectEnabled("Radial Blur", false);
}

} // namespace App
