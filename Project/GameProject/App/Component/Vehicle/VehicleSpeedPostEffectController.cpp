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

float CalculateEffectAmount(float currentSpeed, float minimumEffectSpeed, float maximumEffectSpeed) {
   const float speedRange = maximumEffectSpeed - minimumEffectSpeed;
   if (speedRange <= 1e-4f) {
	  return currentSpeed >= minimumEffectSpeed ? 1.0f : 0.0f;
   }

   return Saturate((currentSpeed - minimumEffectSpeed) / speedRange);
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

   const float currentSpeed = std::max(groundMover->GetCurrentSpeed(), 0.0f);
   const float targetAmount = CalculateEffectAmount(currentSpeed, minimumEffectSpeed, maximumEffectSpeed);
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
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) { return; }

   ImGui::Separator();
   ImGui::DragFloat(Tr("演出最低速度", "Minimum Effect Speed"), &minimumEffectSpeed, 0.1f, 0.0f, 200.0f);
   ImGui::DragFloat(Tr("演出最大速度", "Maximum Effect Speed"), &maximumEffectSpeed, 0.1f, 0.0f, 200.0f);
   ImGui::DragFloat(Tr("追従速度", "Response Speed"), &responseSpeed, 0.1f, 0.0f, 30.0f);
   ImGui::DragFloat(Tr("表示しきい値", "Visible Threshold"), &visibleThreshold, 0.001f, 0.0f, 1.0f);
   ImGui::DragFloat(Tr("待機時内側半径", "Idle Inner Radius"), &idleInnerRadius, 0.01f, 0.0f, 2.0f);
   ImGui::DragFloat(Tr("有効時内側半径", "Active Inner Radius"), &activeInnerRadius, 0.01f, 0.0f, 2.0f);
   ImGui::DragFloat(Tr("外側半径", "Outer Radius"), &outerRadius, 0.01f, 0.0f, 2.0f);
   ImGui::DragFloat(Tr("最大強度", "Max Intensity"), &maxIntensity, 0.01f, 0.0f, 3.0f);
   ImGui::DragFloat(Tr("待機時フロー速度", "Idle Flow Speed"), &idleFlowSpeed, 0.01f, 0.0f, 5.0f);
   ImGui::DragFloat(Tr("有効時フロー速度", "Active Flow Speed"), &activeFlowSpeed, 0.01f, 0.0f, 5.0f);
   ImGui::DragFloat(Tr("ライン密度", "Line Density"), &lineDensity, 1.0f, 16.0f, 512.0f);
   ImGui::DragFloat(Tr("太さ", "Thickness"), &thickness, 0.01f, 0.0f, 1.0f);
   ImGui::DragFloat(Tr("ランダムシード", "Random Seed"), &randomSeed, 0.1f, 0.0f, 100.0f);
   ImGui::DragFloat(Tr("放射ブラー最大値", "Radial Blur Max"), &radialBlurMaxStrength, 0.001f, 0.0f, 0.1f, "%.3f");
   ImGui::SliderInt(Tr("放射ブラーサンプル数", "Radial Samples"), &radialBlurSampleCount, 2, 32);
   ImGui::DragFloat(Tr("放射ブラー表示しきい値", "Radial Visible Threshold"), &radialBlurVisibleThreshold, 0.001f, 0.0f, 1.0f);
   ImGui::Text("%s: %.3f", Tr("エフェクト量", "Effect Amount"), effectAmount_);
}
#endif

nlohmann::json VehicleSpeedPostEffectController::Serialize() const {
   nlohmann::json json;
   json["minimumEffectSpeed"] = minimumEffectSpeed;
   json["maximumEffectSpeed"] = maximumEffectSpeed;
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
   if (data.contains("minimumEffectSpeed")) { minimumEffectSpeed = data["minimumEffectSpeed"]; }
   if (data.contains("maximumEffectSpeed")) { maximumEffectSpeed = data["maximumEffectSpeed"]; }
   if (!data.contains("minimumEffectSpeed") && data.contains("activationMargin")) {
	  float baseSpeed = minimumEffectSpeed;
	  if (HasOwner()) {
		 if (auto* groundMover = GetOwner().GetComponent<VehicleGroundMover>()) {
			baseSpeed = groundMover->autoSpeed;
		 }
	  }
	  minimumEffectSpeed = baseSpeed + std::max(data["activationMargin"].get<float>(), 0.0f);
   }
   if (!data.contains("maximumEffectSpeed") && data.contains("effectSpeedRange")) {
	  maximumEffectSpeed = minimumEffectSpeed + std::max(data["effectSpeedRange"].get<float>(), 0.01f);
   }
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
