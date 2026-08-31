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
// フェードは UIText の不透明度を操作するコンポーネントなので、登録先の型も UIText に限定する。
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
   // 生成直後から有効なオブジェクトでは OnEnable が別途呼ばれない経路もあるため、
   // アタッチ時と再有効化時の双方で自動再生条件を評価する。
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
   // 時計の巻き戻りや異常な負値でアニメーション位相が逆行しないよう、加算値を 0 以上に制限する。
   elapsed_ += std::max(deltaTime, 0.0f);
   // 共通評価器を通し、TransformTweenと同じLoop/PingPongの時間規則を使う。
   const UIPlaybackSample sample = EvaluateUIPlayback(elapsed_, delay, duration, playbackMode);
   Apply(EvaluateUIEasing(sample.progress, easing));
   if (sample.finished) {
      playing_ = false;
   }
}

void UIFadeComponent::Play() {
   // Play は一時停止位置からの再開、Restart は先頭からの再生として役割を分ける。
   playing_ = true;
}

void UIFadeComponent::Pause() {
   playing_ = false;
}

void UIFadeComponent::Restart() {
   elapsed_ = 0.0f;
   playing_ = true;
   // delay 中も開始値が確実に表示されるよう、次回 Update を待たずに初期状態を反映する。
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
   // JSON はエディター外からも編集できるため、補間器が前提とする範囲へ復元時に正規化する。
   // duration は 0 除算を避けられる最小正値を保証し、列挙値は定義済み範囲から出さない。
   startOpacity = std::clamp(data.value("startOpacity", startOpacity), 0.0f, 1.0f);
   endOpacity = std::clamp(data.value("endOpacity", endOpacity), 0.0f, 1.0f);
   delay = std::max(data.value("delay", delay), 0.0f);
   duration = std::max(data.value("duration", duration), 0.0001f);
   playOnEnable = data.value("playOnEnable", playOnEnable);
   playbackMode = static_cast<UIPlaybackMode>(std::clamp(
      data.value("playbackMode", static_cast<int>(playbackMode)),
      static_cast<int>(UIPlaybackMode::Once),
      static_cast<int>(UIPlaybackMode::PingPong)));
   easing = static_cast<UIEasingType>(std::clamp(
      data.value("easing", static_cast<int>(easing)),
      static_cast<int>(UIEasingType::Linear),
      static_cast<int>(UIEasingType::EaseOutBack)));
   if (playOnEnable) {
      Restart();
   }
}

void UIFadeComponent::Apply(float progress) {
   // 登録対象を UIText に限定しているため、テキスト側のアルファ setter を唯一の反映経路にする。
   // setter を通すことで最終的な 0～1 の制約もコンポーネント境界で維持される。
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
