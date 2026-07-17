#include "pch.h"
#include "TypewriterComponent.h"
#include "Component/ComponentRegistry.h"
#include "Component/UI/UITextComponent.h"
#include "Core/UI/Text/Utf8Decoder.h"
#include "Object.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "imgui.h"
#include "Utility/ImGuiHelper.h"
#endif

namespace {
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::TypewriterComponent::kTypeName,
   [](GameEngine::Object& object) -> GameEngine::IObjectComponent* { return object.AddComponent<GameEngine::TypewriterComponent>(); },
   GameEngine::TypewriterComponent::kDisplayName,
   GameEngine::ToObjectTypeMask(GameEngine::ObjectType::UIText));
}

namespace GameEngine {

const char* TypewriterComponent::GetTypeName() const {
   return kTypeName;
}

void TypewriterComponent::OnAttach() {
   if (playOnEnable) {
      Restart();
   }
}

void TypewriterComponent::OnEnable() {
   if (playOnEnable) {
      Restart();
   }
}

void TypewriterComponent::Update(float deltaTime) {
   auto* textComponent = GetOwner().GetComponent<UITextComponent>();
   if (!textComponent) {
      return;
   }

   if (restartOnTextChange && observedTextRevision_ != textComponent->GetTextRevision()) {
      Restart();
   }
   if (!playing_) {
      return;
   }

   const size_t glyphCount = CountGlyphs();
   if (glyphCount == 0) {
      textComponent->SetVisibleGlyphCount(0);
      playing_ = false;
      return;
   }

   elapsed_ += (std::max)(deltaTime, 0.0f);
   if (elapsed_ <= delay) {
      textComponent->SetVisibleGlyphCount(0);
      return;
   }

   const float safeRate = (std::max)(glyphsPerSecond, 0.001f);
   const float revealTime = static_cast<float>(glyphCount) / safeRate;
   float animationTime = elapsed_ - (std::max)(delay, 0.0f);
   if (loop && animationTime >= revealTime) {
      animationTime = std::fmod(animationTime, revealTime);
   }

   const size_t visibleCount = (std::min)(
      static_cast<size_t>(std::floor(animationTime * safeRate)), glyphCount);
   textComponent->SetVisibleGlyphCount(visibleCount);
   if (!loop && visibleCount >= glyphCount) {
      playing_ = false;
   }
}

void TypewriterComponent::Play() {
   playing_ = true;
}

void TypewriterComponent::Pause() {
   playing_ = false;
}

void TypewriterComponent::Restart() {
   elapsed_ = 0.0f;
   playing_ = true;
   if (auto* textComponent = GetOwner().GetComponent<UITextComponent>()) {
      observedTextRevision_ = textComponent->GetTextRevision();
      textComponent->SetVisibleGlyphCount(0);
   }
}

void TypewriterComponent::Complete() {
   playing_ = false;
   if (auto* textComponent = GetOwner().GetComponent<UITextComponent>()) {
      observedTextRevision_ = textComponent->GetTextRevision();
      textComponent->SetVisibleGlyphCount(UITextComponent::kShowAllGlyphs);
   }
}

size_t TypewriterComponent::CountGlyphs() const {
   const auto* textComponent = GetOwner().GetComponent<UITextComponent>();
   return textComponent ? CountRenderableCodePoints(textComponent->GetText()) : 0;
}

nlohmann::json TypewriterComponent::Serialize() const {
   return nlohmann::json{
      { "glyphsPerSecond", glyphsPerSecond },
      { "delay", delay },
      { "playOnEnable", playOnEnable },
      { "loop", loop },
      { "restartOnTextChange", restartOnTextChange }
   };
}

void TypewriterComponent::Deserialize(const nlohmann::json& data) {
   glyphsPerSecond = (std::max)(data.value("glyphsPerSecond", glyphsPerSecond), 0.001f);
   delay = (std::max)(data.value("delay", delay), 0.0f);
   playOnEnable = data.value("playOnEnable", playOnEnable);
   loop = data.value("loop", loop);
   restartOnTextChange = data.value("restartOnTextChange", restartOnTextChange);
   if (playOnEnable) {
      Restart();
   }
}

#ifdef USE_IMGUI
void TypewriterComponent::DrawInspector() {
   const std::string header = MakeObjectComponentHeaderLabel(GetTypeName());
   if (!ImGui::CollapsingHeader(header.c_str())) return;
   if (ImGui::Button(ImGuiHelper::Localize({ "再生", "Play" }))) Play();
   ImGui::SameLine();
   if (ImGui::Button(ImGuiHelper::Localize({ "一時停止", "Pause" }))) Pause();
   ImGui::SameLine();
   if (ImGui::Button(ImGuiHelper::Localize({ "リスタート", "Restart" }))) Restart();
   ImGui::SameLine();
   if (ImGui::Button(ImGuiHelper::Localize({ "即時表示", "Complete" }))) Complete();
   ImGui::DragFloat(ImGuiHelper::Localize({ "1秒あたりの文字数", "Glyphs Per Second" }), &glyphsPerSecond, 0.1f, 0.001f);
   ImGui::DragFloat(ImGuiHelper::Localize({ "遅延", "Delay" }), &delay, 0.01f, 0.0f);
   ImGui::Checkbox(ImGuiHelper::Localize({ "有効化時に再生", "Play On Enable" }), &playOnEnable);
   ImGui::Checkbox(ImGuiHelper::Localize({ "ループ", "Loop" }), &loop);
   ImGui::Checkbox(ImGuiHelper::Localize({ "文字変更時に再開", "Restart On Text Change" }), &restartOnTextChange);
   ImGui::Spacing();
}
#endif

} // namespace GameEngine
