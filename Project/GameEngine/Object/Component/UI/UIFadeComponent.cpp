#include "pch.h"
#include "UIFadeComponent.h"
#include "Component/ComponentRegistry.h"
#include "Component/UI/UITextComponent.h"
#include "Object.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "imgui.h"
#include "Utility/ImGuiHelper.h"
#endif

namespace {
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::UIFadeComponent::kTypeName,
   [](GameEngine::Object& object) -> GameEngine::IObjectComponent* { return object.AddComponent<GameEngine::UIFadeComponent>(); },
   GameEngine::UIFadeComponent::kDisplayName,
   GameEngine::ToObjectTypeMask(GameEngine::ObjectType::UIText));
}

namespace GameEngine {

const char* UIFadeComponent::GetTypeName() const {
   return kTypeName;
}

void UIFadeComponent::OnAttach() {
   if (playOnEnable) {
      Restart();
   }
}

void UIFadeComponent::OnEnable() {
   if (playOnEnable) {
      Restart();
   }
}

void UIFadeComponent::Update(float deltaTime) {
   if (!playing_) {
      return;
   }
   elapsed_ += std::max(deltaTime, 0.0f);
   const UIPlaybackSample sample = EvaluateUIPlayback(elapsed_, delay, duration, playbackMode);
   Apply(EvaluateUIEasing(sample.progress, easing));
   if (sample.finished) {
      playing_ = false;
   }
}

void UIFadeComponent::Play() {
   playing_ = true;
}

void UIFadeComponent::Pause() {
   playing_ = false;
}

void UIFadeComponent::Restart() {
   elapsed_ = 0.0f;
   playing_ = true;
   Apply(0.0f);
}

nlohmann::json UIFadeComponent::Serialize() const {
   return nlohmann::json{
      { "startOpacity", startOpacity },
      { "endOpacity", endOpacity },
      { "delay", delay },
      { "duration", duration },
      { "playOnEnable", playOnEnable },
      { "playbackMode", static_cast<int>(playbackMode) },
      { "easing", static_cast<int>(easing) }
   };
}

void UIFadeComponent::Deserialize(const nlohmann::json& data) {
   startOpacity = std::clamp(data.value("startOpacity", startOpacity), 0.0f, 1.0f);
   endOpacity = std::clamp(data.value("endOpacity", endOpacity), 0.0f, 1.0f);
   delay = std::max(data.value("delay", delay), 0.0f);
   duration = std::max(data.value("duration", duration), 0.0001f);
   playOnEnable = data.value("playOnEnable", playOnEnable);
   playbackMode = static_cast<UIPlaybackMode>(std::clamp(data.value("playbackMode", static_cast<int>(playbackMode)), 0, 2));
   easing = static_cast<UIEasingType>(std::clamp(data.value("easing", static_cast<int>(easing)), 0, 3));
   if (playOnEnable) {
      Restart();
   }
}

void UIFadeComponent::Apply(float progress) {
   if (auto* textComponent = GetOwner().GetComponent<UITextComponent>()) {
      textComponent->SetOpacity(startOpacity + (endOpacity - startOpacity) * progress);
   }
}

#ifdef USE_IMGUI
void UIFadeComponent::DrawInspector() {
   const std::string header = MakeObjectComponentHeaderLabel(GetTypeName());
   if (!ImGui::CollapsingHeader(header.c_str())) return;
   if (ImGui::Button(ImGuiHelper::Localize({ "再生", "Play" }))) Play();
   ImGui::SameLine();
   if (ImGui::Button(ImGuiHelper::Localize({ "一時停止", "Pause" }))) Pause();
   ImGui::SameLine();
   if (ImGui::Button(ImGuiHelper::Localize({ "リスタート", "Restart" }))) Restart();
   ImGui::SliderFloat(ImGuiHelper::Localize({ "開始不透明度", "Start Opacity" }), &startOpacity, 0.0f, 1.0f);
   ImGui::SliderFloat(ImGuiHelper::Localize({ "終了不透明度", "End Opacity" }), &endOpacity, 0.0f, 1.0f);
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
