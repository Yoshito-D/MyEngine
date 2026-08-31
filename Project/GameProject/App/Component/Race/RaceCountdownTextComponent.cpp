#include "RaceCountdownTextComponent.h"

#include "RaceManagerComponent.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Component/UI/UITextComponent.h"
#include "Object/Object.h"
#include "Scene/SceneWorld.h"
#include "Utility/MathUtils.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void RaceCountdownTextComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   // シーン再読み込み後に以前のRaceManagerを参照しないよう、保存済みIDから解決し直す。
   raceManager_ = nullptr;
   if (auto* managerObject = sceneWorld.FindObjectById(raceManagerId_)) {
      raceManager_ = managerObject->GetComponent<RaceManagerComponent>();
   }
   // アニメーションで上書きする前の見た目を、シーンに設定された復元先として記録する。
   CaptureBaseVisualState();
   displayedText_.clear();
   animationElapsed_ = 0.0f;
}

void RaceCountdownTextComponent::Update(float deltaTime) {
   if (!raceManager_ || !HasOwner()) {
      return;
   }
   auto* text = GetOwner().GetComponent<GameEngine::UITextComponent>();
   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   if (!text || !transform) {
      return;
   }

   // Countdown中は残り秒数、Running直後の表示期間はSTART、それ以外は空文字とし、
   // 一つのUITextをレース状態に応じて排他的に使う。
   std::string nextText;
   if (raceManager_->GetState() == RaceManagerComponent::State::Countdown) {
      // 切り上げにより残り時間が正の間は0を表示せず、GO表示との境界を明確にする。
      const int count = std::max(1, static_cast<int>(std::ceil(raceManager_->GetCountdownRemaining())));
      nextText = std::to_string(count);
   } else if (raceManager_->IsStartBannerVisible()) {
      nextText = startText_;
   }

   text->SetText(nextText);
   if (nextText.empty()) {
      // 非表示期間にもシーンで設定された姿勢と透明度へ戻し、次回表示の開始状態を保証する。
      RestoreBaseVisualState(*text, *transform);
      displayedText_.clear();
      animationElapsed_ = 0.0f;
      return;
   }

   if (nextText != displayedText_) {
      // 秒境界で必ず初期姿勢へ戻し、フレーム落ちで前の数字のフェード状態を引き継がない。
      RestoreBaseVisualState(*text, *transform);
      displayedText_ = std::move(nextText);
      animationElapsed_ = 0.0f;
   } else {
      animationElapsed_ += std::max(deltaTime, 0.0f);
   }
   ApplyAnimation(*text, *transform);
}

nlohmann::json RaceCountdownTextComponent::Serialize() const {
   // 表示中の文字や経過時間は保存せず、再読み込み時にレース状態から再構築できる設定だけを残す。
   return nlohmann::json{
      { "raceManagerId", raceManagerId_ },
      { "startText", startText_ },
      { "rotationDuration", rotationDuration_ },
      { "fadeDuration", fadeDuration_ },
      { "fadeEndScale", fadeEndScale_ }
   };
}

void RaceCountdownTextComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }
   // 各項目を独立して検証し、欠落・型不一致の項目は既定値または現在値を維持する。
   if (data.contains("raceManagerId") && data.at("raceManagerId").is_string()) {
      raceManagerId_ = data.at("raceManagerId").get<std::string>();
   }
   if (data.contains("startText") && data.at("startText").is_string()) {
      startText_ = data.at("startText").get<std::string>();
   }
   // 両時間はアニメーション進捗の除数になるため、0以下を許可しない。
   if (data.contains("rotationDuration") && data.at("rotationDuration").is_number()) {
      rotationDuration_ = std::max(data.at("rotationDuration").get<float>(), 0.0001f);
   }
   if (data.contains("fadeDuration") && data.at("fadeDuration").is_number()) {
      fadeDuration_ = std::max(data.at("fadeDuration").get<float>(), 0.0001f);
   }
   if (data.contains("fadeEndScale") && data.at("fadeEndScale").is_number()) {
      // 終端倍率を1以上に制限し、カウントダウン演出が意図せず縮小へ反転するのを防ぐ。
      fadeEndScale_ = std::max(data.at("fadeEndScale").get<float>(), 1.0f);
   }
}

void RaceCountdownTextComponent::CaptureBaseVisualState() {
   // 再取得に失敗した場合に以前のシーンの退避値を使わないよう、有効フラグを先に落とす。
   hasBaseVisualState_ = false;
   if (!HasOwner()) {
      return;
   }
   const auto* text = GetOwner().GetComponent<GameEngine::UITextComponent>();
   const auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   if (!text || !transform) {
      return;
   }

   baseOpacity_ = text->GetStyle().color.w;
   baseScale_ = transform->transform.scale;
   // 元の回転表現を変えないよう、Euler値とQuaternion値の両方を退避する。
   baseEuler_ = transform->transform.GetActiveEuler();
   baseRotationQuaternion_ = transform->transform.GetActiveQuaternion();
   baseUsesQuaternion_ = transform->transform.IsUsingQuaternion();
   hasBaseVisualState_ = true;
}

void RaceCountdownTextComponent::RestoreBaseVisualState(
   GameEngine::UITextComponent& text,
   GameEngine::TransformComponent& transform) {
   if (!hasBaseVisualState_) {
      // シーンロード順などで未取得なら、利用時点で一度だけ現在の見た目を退避する。
      CaptureBaseVisualState();
   }
   if (!hasBaseVisualState_) {
      return;
   }

   text.SetOpacity(baseOpacity_);
   transform.transform.scale = baseScale_;
   if (baseUsesQuaternion_) {
      transform.transform.SetRotationQuaternion(baseRotationQuaternion_);
   } else {
      transform.transform.SetRotationEuler(baseEuler_);
   }
}

void RaceCountdownTextComponent::ApplyAnimation(
   GameEngine::UITextComponent& text,
   GameEngine::TransformComponent& transform) {
   if (!hasBaseVisualState_) {
      return;
   }

   const float rotationProgress = std::clamp(animationElapsed_ / rotationDuration_, 0.0f, 1.0f);
   // 基準姿勢へ毎フレーム加算し、前フレームの回転誤差を累積させない。
   const float easedRotation = GameEngine::Easing::EaseOutCubic(0.0f, 1.0f, rotationProgress);
   GameEngine::Vector3 animatedEuler = baseEuler_;
   animatedEuler.z += GameEngine::MathConstants::kTwoPi * easedRotation;
   transform.transform.SetRotationEuler(animatedEuler);

   // 一回転を終えてから拡大と透明化を始め、数字ごとの動きを明確に分離する。
   const float fadeProgress = std::clamp(
      (animationElapsed_ - rotationDuration_) / fadeDuration_,
      0.0f,
      1.0f);
   const float scaleMultiplier = GameEngine::Easing::EaseOutCubic(1.0f, fadeEndScale_, fadeProgress);
   transform.transform.scale = {
      baseScale_.x * scaleMultiplier,
      baseScale_.y * scaleMultiplier,
      baseScale_.z
   };
   text.SetOpacity(GameEngine::Easing::EaseInQuad(baseOpacity_, 0.0f, fadeProgress));
}

#ifdef USE_IMGUI
void RaceCountdownTextComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Text("Race Manager ID: %s", raceManagerId_.c_str());
   ImGui::Text("Resolved: %s", raceManager_ ? "true" : "false");
   ImGui::DragFloat("Rotation Duration", &rotationDuration_, 0.01f, 0.01f, 1.0f, "%.2f s");
   ImGui::DragFloat("Fade Duration", &fadeDuration_, 0.01f, 0.01f, 1.0f, "%.2f s");
   ImGui::DragFloat("Fade End Scale", &fadeEndScale_, 0.01f, 1.0f, 3.0f, "%.2f");
}
#endif

} // namespace App
