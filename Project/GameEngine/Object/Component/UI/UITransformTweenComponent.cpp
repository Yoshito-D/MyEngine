#include "pch.h"
#include "UITransformTweenComponent.h"
#include "Component/ComponentRegistry.h"
#include "Component/TransformComponent.h"
#include "Object.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "imgui.h"
#include "Utility/ImGuiHelper.h"
#endif

namespace {
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::UITransformTweenComponent::kTypeName,
   [](GameEngine::Object& object) -> GameEngine::IObjectComponent* { return object.AddComponent<GameEngine::UITransformTweenComponent>(); },
   GameEngine::UITransformTweenComponent::kDisplayName,
   GameEngine::ObjectType::Sprite | GameEngine::ObjectType::UIText);
}

namespace GameEngine {

const char* UITransformTweenComponent::GetTypeName() const {
   return kTypeName;
}

void UITransformTweenComponent::OnAttach() {
   if (playOnEnable) {
      Restart();
   }
}

void UITransformTweenComponent::OnEnable() {
   if (playOnEnable) {
      Restart();
   }
}

void UITransformTweenComponent::Update(float deltaTime) {
   if (!playing_) {
      return;
   }
   elapsed_ += (std::max)(deltaTime, 0.0f);
   const UIPlaybackSample sample = EvaluateUIPlayback(elapsed_, delay, duration, playbackMode);
   Apply(EvaluateUIEasing(sample.progress, easing));
   if (sample.finished) {
      playing_ = false;
   }
}

void UITransformTweenComponent::Play() {
   playing_ = true;
}

void UITransformTweenComponent::Pause() {
   playing_ = false;
}

void UITransformTweenComponent::Restart() {
   elapsed_ = 0.0f;
   playing_ = true;
   Apply(EvaluateUIEasing(0.0f, easing));
}

nlohmann::json UITransformTweenComponent::Serialize() const {
   return nlohmann::json{
      { "animatePosition", animatePosition },
      { "animateScale", animateScale },
      { "animateRotation", animateRotation },
      { "startPosition", { startPosition.x, startPosition.y } },
      { "endPosition", { endPosition.x, endPosition.y } },
      { "startScale", { startScale.x, startScale.y } },
      { "endScale", { endScale.x, endScale.y } },
      { "startRotation", startRotation },
      { "endRotation", endRotation },
      { "delay", delay },
      { "duration", duration },
      { "playOnEnable", playOnEnable },
      { "playbackMode", static_cast<int>(playbackMode) },
      { "easing", static_cast<int>(easing) }
   };
}

void UITransformTweenComponent::Deserialize(const nlohmann::json& data) {
   animatePosition = data.value("animatePosition", animatePosition);
   animateScale = data.value("animateScale", animateScale);
   animateRotation = data.value("animateRotation", animateRotation);
   if (data.contains("startPosition") && data.at("startPosition").is_array() && data.at("startPosition").size() >= 2) {
      startPosition = { data.at("startPosition")[0].get<float>(), data.at("startPosition")[1].get<float>() };
   }
   if (data.contains("endPosition") && data.at("endPosition").is_array() && data.at("endPosition").size() >= 2) {
      endPosition = { data.at("endPosition")[0].get<float>(), data.at("endPosition")[1].get<float>() };
   }
   if (data.contains("startScale") && data.at("startScale").is_array() && data.at("startScale").size() >= 2) {
      startScale = { data.at("startScale")[0].get<float>(), data.at("startScale")[1].get<float>() };
   }
   if (data.contains("endScale") && data.at("endScale").is_array() && data.at("endScale").size() >= 2) {
      endScale = { data.at("endScale")[0].get<float>(), data.at("endScale")[1].get<float>() };
   }
   startRotation = data.value("startRotation", startRotation);
   endRotation = data.value("endRotation", endRotation);
   delay = (std::max)(data.value("delay", delay), 0.0f);
   duration = (std::max)(data.value("duration", duration), 0.0001f);
   playOnEnable = data.value("playOnEnable", playOnEnable);
   playbackMode = static_cast<UIPlaybackMode>((std::clamp)(data.value("playbackMode", static_cast<int>(playbackMode)), 0, 2));
   easing = static_cast<UIEasingType>((std::clamp)(data.value("easing", static_cast<int>(easing)), 0, 3));
   if (playOnEnable) {
      Restart();
   }
}

void UITransformTweenComponent::Apply(float progress) {
   auto* transformComponent = GetOwner().GetComponent<TransformComponent>();
   if (!transformComponent) {
      return;
   }
   Transform& transform = transformComponent->transform;
   if (animatePosition) {
      const Vector2 value = startPosition + (endPosition - startPosition) * progress;
      transform.translation.x = value.x;
      transform.translation.y = value.y;
   }
   if (animateScale) {
      const Vector2 value = startScale + (endScale - startScale) * progress;
      transform.scale.x = value.x;
      transform.scale.y = value.y;
   }
   if (animateRotation) {
      Vector3 euler = transform.GetActiveEuler();
      euler.z = startRotation + (endRotation - startRotation) * progress;
      transform.SetRotationEuler(euler);
   }
}

#ifdef USE_IMGUI
void UITransformTweenComponent::DrawInspector() {
   const std::string header = MakeObjectComponentHeaderLabel(GetTypeName());
   if (!ImGui::CollapsingHeader(header.c_str())) return;
   if (ImGui::Button(ImGuiHelper::Localize({ "再生", "Play" }))) Play();
   ImGui::SameLine();
   if (ImGui::Button(ImGuiHelper::Localize({ "一時停止", "Pause" }))) Pause();
   ImGui::SameLine();
   if (ImGui::Button(ImGuiHelper::Localize({ "リスタート", "Restart" }))) Restart();
   ImGui::Checkbox(ImGuiHelper::Localize({ "移動", "Position" }), &animatePosition);
   if (animatePosition) {
      ImGui::DragFloat2("Start Position", &startPosition.x);
      ImGui::DragFloat2("End Position", &endPosition.x);
   }
   ImGui::Checkbox(ImGuiHelper::Localize({ "拡縮", "Scale" }), &animateScale);
   if (animateScale) {
      ImGui::DragFloat2("Start Scale", &startScale.x, 0.01f);
      ImGui::DragFloat2("End Scale", &endScale.x, 0.01f);
   }
   ImGui::Checkbox(ImGuiHelper::Localize({ "回転", "Rotation" }), &animateRotation);
   if (animateRotation) {
      ImGui::DragFloat("Start Rotation", &startRotation, 0.01f);
      ImGui::DragFloat("End Rotation", &endRotation, 0.01f);
   }
   ImGui::DragFloat(ImGuiHelper::Localize({ "遅延", "Delay" }), &delay, 0.01f, 0.0f);
   ImGui::DragFloat(ImGuiHelper::Localize({ "時間", "Duration" }), &duration, 0.01f, 0.0001f);
   ImGui::Checkbox(ImGuiHelper::Localize({ "有効化時に再生", "Play On Enable" }), &playOnEnable);
   int mode = static_cast<int>(playbackMode);
   const char* modes[] = { "Once", "Loop", "Ping Pong" };
   if (ImGui::Combo("Playback", &mode, modes, 3)) playbackMode = static_cast<UIPlaybackMode>(mode);
   int easingIndex = static_cast<int>(easing);
   const char* easings[] = { "Linear", "Ease In Out Sine", "Ease Out Cubic", "Ease Out Back" };
   if (ImGui::Combo("Easing", &easingIndex, easings, 4)) easing = static_cast<UIEasingType>(easingIndex);
   ImGui::Spacing();
}
#endif

} // namespace GameEngine
