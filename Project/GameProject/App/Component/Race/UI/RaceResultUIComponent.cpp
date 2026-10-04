#include "GameProject/App/Component/Race/UI/RaceResultUIComponent.h"

#include "GameProject/App/Component/Race/RaceManagerComponent.h"
#include "GameProject/App/Component/Race/UI/RaceTimeFormatting.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Component/UI/UITextComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Scene/BaseScene.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/Math/MathUtils.h"
#include <algorithm>
#include <cmath>
#include <sstream>

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace App {

void RaceResultUIComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   resultRequested_ = false;
   navigationLatched_ = false;
   showingResult_ = false;
   selectedOption_ = 1;
   reactionElapsed_ = 0.0f;
   selectionAnimationElapsed_ = 0.0f;
   selectionSwitchElapsed_ = 0.0f;
   reactionStartScale_ = { 1.0f, 1.0f, 1.0f };
   selectionSwitchActive_ = false;
   hasBaseVisualStates_ = false;
   optionTexts_.fill(nullptr);
   optionTransforms_.fill(nullptr);
   // 実行時ロードではOnReferencesChangedが自動では呼ばれないため、
   // RaceManagerと選択肢の参照を同じ経路で解決する。
   OnReferencesChanged(sceneWorld);
   hasBaseVisualStates_ = CaptureBaseVisualStates();
   HideOptionVisuals();
}

void RaceResultUIComponent::OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) {
   // シーン再読み込み後に破棄済みオブジェクトを参照しないよう、
   // 前回解決したポインターを捨てて保存済みIDから解決し直す。
   raceManager_ = nullptr;
   optionTexts_.fill(nullptr);
   optionTransforms_.fill(nullptr);
   if (auto* managerObject = sceneWorld.FindObjectById(raceManagerId_)) {
      raceManager_ = managerObject->GetComponent<RaceManagerComponent>();
   }
   ResolveOptionVisuals(sceneWorld);
}

void RaceResultUIComponent::Update(float deltaTime) {
   if (!raceManager_ || !HasOwner()) {
      return;
   }
   auto* text = GetOwner().GetComponent<GameEngine::UITextComponent>();
   if (!text) {
      return;
   }

   // このUITextはレース中もシーンに残るため、終了状態以外では内容を消しておく。
   // これによりリスタート後に前回のリザルトが一瞬残ることも防ぐ。
   if (raceManager_->GetState() != RaceManagerComponent::State::Finished) {
      text->SetText("");
      if (showingResult_ || resultRequested_) {
         showingResult_ = false;
         resultRequested_ = false;
         selectedOption_ = 1;
         navigationLatched_ = false;
         selectionAnimationElapsed_ = 0.0f;
         selectionSwitchElapsed_ = 0.0f;
         selectionSwitchActive_ = false;
      }
      HideOptionVisuals();
      return;
   }

   if (!showingResult_) {
      showingResult_ = true;
      resultRequested_ = false;
      selectedOption_ = IsOptionAvailable(1) ? 1 : 0;
      navigationLatched_ = false;
      selectionAnimationElapsed_ = 0.0f;
      selectionSwitchElapsed_ = 0.0f;
      selectionSwitchActive_ = false;
      RefreshSelectionText();
   }

   text->SetText(BuildResultText());
   if (resultRequested_) {
      reactionElapsed_ += std::max(deltaTime, 0.0f);
      ApplyResultReaction(static_cast<std::size_t>(selectedOption_));
      return;
   }

   bool selectionChanged = false;
   const auto& navigationAction =
      GameEngine::EngineContext::GetInputActionState("UI", "UI.NavigateHorizontal", 0);
   const int navigationDirection = navigationAction.value.x > 0.5f
      ? 1
      : (navigationAction.value.x < -0.5f ? -1 : 0);
   // 閾値未満を中立として扱い、スティックの微小な揺れで選択が変わらないようにする。
   if (navigationDirection == 0) {
      navigationLatched_ = false;
   } else if (!navigationLatched_) {
      selectionChanged = MoveSelection(navigationDirection);
      navigationLatched_ = true;
      if (selectionChanged) {
         selectionAnimationElapsed_ = 0.0f;
         selectionSwitchElapsed_ = 0.0f;
         selectionSwitchActive_ = true;
         RefreshSelectionText();
      }
   }
   // 切り替えたフレームは縮小の開始値を必ず一度表示する。
   UpdateSelectionAnimation(selectionChanged ? 0.0f : deltaTime);

   const auto& confirmAction =
      GameEngine::EngineContext::GetInputActionState("UI", "UI.Confirm", 0);
   if (confirmAction.triggered) {
      ConfirmSelection();
   }
}

nlohmann::json RaceResultUIComponent::Serialize() const {
   // 実行時ポインターはシーンをまたいで有効ではないため、再解決用IDだけを永続化する。
   return nlohmann::json{
      { "raceManagerId", raceManagerId_ },
      { "titleOptionObjectId", titleOptionObjectId_ },
      { "restartOptionObjectId", restartOptionObjectId_ },
      { "titleScene", titleScene_ },
      { "reactionDuration", reactionDuration_ },
      { "reactionEndScale", reactionEndScale_ },
      { "selectionPulseDuration", selectionPulseDuration_ },
      { "selectionPulseScale", selectionPulseScale_ },
      { "selectionSwitchDuration", selectionSwitchDuration_ },
      { "selectionSwitchScale", selectionSwitchScale_ }
   };
}

void RaceResultUIComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }
   // 欠落または型不一致の値では現在の設定を維持し、旧形式や部分設定も受け入れる。
   if (data.contains("raceManagerId") && data.at("raceManagerId").is_string()) {
      raceManagerId_ = data.at("raceManagerId").get<std::string>();
   }
   if (data.contains("titleOptionObjectId") && data.at("titleOptionObjectId").is_string()) {
      titleOptionObjectId_ = data.at("titleOptionObjectId").get<std::string>();
   }
   if (data.contains("restartOptionObjectId") && data.at("restartOptionObjectId").is_string()) {
      restartOptionObjectId_ = data.at("restartOptionObjectId").get<std::string>();
   }
   if (data.contains("titleScene") && data.at("titleScene").is_string()) {
      titleScene_ = data.at("titleScene").get<std::string>();
   }
   if (data.contains("reactionDuration") && data.at("reactionDuration").is_number()) {
      reactionDuration_ = std::max(data.at("reactionDuration").get<float>(), 0.0001f);
   }
   if (data.contains("reactionEndScale") && data.at("reactionEndScale").is_number()) {
      reactionEndScale_ = std::max(data.at("reactionEndScale").get<float>(), 1.0f);
   }
   if (data.contains("selectionPulseDuration") && data.at("selectionPulseDuration").is_number()) {
      selectionPulseDuration_ = std::max(data.at("selectionPulseDuration").get<float>(), 0.0001f);
   }
   if (data.contains("selectionPulseScale") && data.at("selectionPulseScale").is_number()) {
      selectionPulseScale_ = std::max(data.at("selectionPulseScale").get<float>(), 1.0f);
   }
   if (data.contains("selectionSwitchDuration") && data.at("selectionSwitchDuration").is_number()) {
      selectionSwitchDuration_ = std::max(data.at("selectionSwitchDuration").get<float>(), 0.0001f);
   }
   if (data.contains("selectionSwitchScale") && data.at("selectionSwitchScale").is_number()) {
      selectionSwitchScale_ = std::clamp(data.at("selectionSwitchScale").get<float>(), 0.0f, 1.0f);
   }
}

void RaceResultUIComponent::ResolveOptionVisuals(GameEngine::SceneWorld& sceneWorld) {
   // 選択肢ごとの設定IDを同じ添字のText・Transform配列へ対応付ける。
   const std::array<std::string, kOptionCount> objectIds = {
      titleOptionObjectId_,
      restartOptionObjectId_
   };
   for (std::size_t optionIndex = 0; optionIndex < objectIds.size(); ++optionIndex) {
      if (auto* optionObject = sceneWorld.FindObjectById(objectIds[optionIndex])) {
         optionTexts_[optionIndex] = optionObject->GetComponent<GameEngine::UITextComponent>();
         optionTransforms_[optionIndex] = optionObject->GetComponent<GameEngine::TransformComponent>();
      }
   }
}

bool RaceResultUIComponent::CaptureBaseVisualStates() {
   bool capturedAny = false;
   for (std::size_t optionIndex = 0; optionIndex < kOptionCount; ++optionIndex) {
      const auto* text = optionTexts_[optionIndex];
      const auto* transform = optionTransforms_[optionIndex];
      if (!text || !transform) {
         continue;
      }
      baseOpacities_[optionIndex] = text->GetStyle().color.w;
      baseScales_[optionIndex] = transform->GetLocalPose().scale;
      capturedAny = true;
   }
   return capturedAny;
}

bool RaceResultUIComponent::IsOptionAvailable(std::size_t optionIndex) const {
   return optionIndex < kOptionCount &&
      optionTexts_[optionIndex] != nullptr && optionTransforms_[optionIndex] != nullptr;
}

bool RaceResultUIComponent::MoveSelection(int direction) {
   const int previousSelection = selectedOption_;
   int candidate = selectedOption_;
   // 端では巡回し、未解決の項目を飛ばす。候補数までの試行に制限して全欠落時も終了させる。
   for (std::size_t attempt = 0; attempt < kOptionCount; ++attempt) {
      candidate = (candidate + direction + static_cast<int>(kOptionCount)) % static_cast<int>(kOptionCount);
      if (IsOptionAvailable(static_cast<std::size_t>(candidate))) {
         selectedOption_ = candidate;
         return selectedOption_ != previousSelection;
      }
   }
   return false;
}

void RaceResultUIComponent::RefreshSelectionText() {
   if (IsOptionAvailable(0)) {
      optionTexts_[0]->SetText(
         std::string(selectedOption_ == 0 ? "> " : "  ") + "TITLE");
   }
   if (IsOptionAvailable(1)) {
      optionTexts_[1]->SetText(
         std::string(selectedOption_ == 1 ? "> " : "  ") + "RESTART");
   }
}

void RaceResultUIComponent::UpdateSelectionAnimation(float deltaTime) {
   const std::size_t selectedIndex = static_cast<std::size_t>(selectedOption_);
   for (std::size_t optionIndex = 0; optionIndex < kOptionCount; ++optionIndex) {
      if (!IsOptionAvailable(optionIndex)) {
         continue;
      }

      const auto& baseScale = baseScales_[optionIndex];
      float scaleMultiplier = 1.0f;
      if (optionIndex == selectedIndex) {
         if (selectionSwitchActive_) {
            selectionSwitchElapsed_ += std::max(deltaTime, 0.0f);
            const float switchProgress =
               std::clamp(selectionSwitchElapsed_ / selectionSwitchDuration_, 0.0f, 1.0f);
            scaleMultiplier = GameEngine::Easing::EaseOutCubic(
               selectionSwitchScale_, 1.0f, switchProgress);
            if (switchProgress >= 1.0f) {
               selectionSwitchActive_ = false;
               selectionAnimationElapsed_ = 0.0f;
            }
         } else {
            selectionAnimationElapsed_ += std::max(deltaTime, 0.0f);
            constexpr float kTwoPi = 6.28318530717958647692f;
            const float pulsePhase =
               std::fmod(selectionAnimationElapsed_, selectionPulseDuration_) / selectionPulseDuration_;
            const float pulseProgress = 0.5f - 0.5f * std::cos(kTwoPi * pulsePhase);
            scaleMultiplier = GameEngine::Easing::EaseInOutSine(
               1.0f, selectionPulseScale_, pulseProgress);
         }
      }

      // 未選択側も基準値から毎フレーム設定し、直前までの選択アニメーションを残さない。
      { auto pose = optionTransforms_[optionIndex]->GetLocalPose(); pose.scale = {
         baseScale.x * scaleMultiplier,
         baseScale.y * scaleMultiplier,
         baseScale.z
      }; optionTransforms_[optionIndex]->ApplyLocalPose(pose); }
   }
}

void RaceResultUIComponent::HideOptionVisuals() {
   for (std::size_t optionIndex = 0; optionIndex < kOptionCount; ++optionIndex) {
      if (!IsOptionAvailable(optionIndex)) {
         continue;
      }
      optionTexts_[optionIndex]->SetText("");
      optionTexts_[optionIndex]->SetOpacity(baseOpacities_[optionIndex]);
      {
         auto pose = optionTransforms_[optionIndex]->GetLocalPose();
         pose.scale = baseScales_[optionIndex];
         optionTransforms_[optionIndex]->ApplyLocalPose(pose);
      }
   }
}

void RaceResultUIComponent::ApplyResultReaction(std::size_t optionIndex) {
   if (!IsOptionAvailable(optionIndex)) {
      return;
   }

   auto& text = *optionTexts_[optionIndex];
   auto& transform = *optionTransforms_[optionIndex];
   const auto& baseScale = baseScales_[optionIndex];
   const float progress = std::clamp(reactionElapsed_ / reactionDuration_, 0.0f, 1.0f);
   const float scaleProgress = GameEngine::Easing::EaseOutCubic(0.0f, 1.0f, progress);
   auto pose = transform.GetLocalPose();
   pose.scale = {
      reactionStartScale_.x + (baseScale.x * reactionEndScale_ - reactionStartScale_.x) * scaleProgress,
      reactionStartScale_.y + (baseScale.y * reactionEndScale_ - reactionStartScale_.y) * scaleProgress,
      baseScale.z
   };
   text.SetOpacity(GameEngine::Easing::EaseInQuad(baseOpacities_[optionIndex], 0.0f, progress));
   transform.ApplyLocalPose(pose);
}

bool RaceResultUIComponent::ConfirmSelection() {
   // オプションUIが存在しない旧シーンでは、従来どおり決定をリスタートへ戻す。
   if (!IsOptionAvailable(0) && !IsOptionAvailable(1)) {
      return raceManager_->RequestRestart();
   }
   if (!IsOptionAvailable(static_cast<std::size_t>(selectedOption_))) {
      return false;
   }

   if (selectedOption_ == 0) {
      if (titleScene_.empty()) {
         return false;
      }
      GameEngine::BaseScene::SetNextSceneName(titleScene_);
   } else if (!raceManager_->RequestRestart()) {
      return false;
   }

   resultRequested_ = true;
   reactionElapsed_ = 0.0f;
   reactionStartScale_ = optionTransforms_[static_cast<std::size_t>(selectedOption_)]->GetLocalPose().scale;
   return true;
}

std::string RaceResultUIComponent::BuildResultText() const {
   std::ostringstream stream;
   stream << "FINISH\nTIME " << FormatRaceTime(raceManager_->GetElapsedTime())
      << "\n\nBEST TIMES\n";

   // RaceManagerが保持する昇順の記録を、その並びを崩さず順位表示へ変換する。
   const auto& bestTimes = raceManager_->GetBestTimes();
   // 記録数にかかわらず固定枠を表示し、リザルトUIの高さが変動しないようにする。
   for (size_t index = 0; index < RaceManagerComponent::kBestTimeCount; ++index) {
      stream << index + 1 << ". ";
      if (index < bestTimes.size()) {
         stream << FormatRaceTime(bestTimes[index]);
      } else {
         stream << "--:--.---";
      }
      stream << '\n';
   }
   if (!IsOptionAvailable(0) && !IsOptionAvailable(1)) {
      stream << "\n> RESTART\nA / SPACE";
   }
   return stream.str();
}

#ifdef USE_IMGUI
void RaceResultUIComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   GameEngine::EditorUI::ObjectReference("Race Manager", raceManagerId_, "RaceManagerComponent");
   GameEngine::EditorUI::ObjectReference("Title Option", titleOptionObjectId_, "UITextComponent");
   GameEngine::EditorUI::ObjectReference("Restart Option", restartOptionObjectId_, "UITextComponent");
   GameEngine::EditorUI::SceneReference("Title Scene", titleScene_);
   ImGui::DragFloat("Reaction Duration", &reactionDuration_, 0.01f, 0.01f, 1.0f, "%.2f s");
   ImGui::DragFloat("Reaction End Scale", &reactionEndScale_, 0.01f, 1.0f, 3.0f, "%.2f");
   ImGui::DragFloat("Selection Pulse Duration", &selectionPulseDuration_, 0.01f, 0.01f, 3.0f, "%.2f s");
   ImGui::DragFloat("Selection Pulse Scale", &selectionPulseScale_, 0.01f, 1.0f, 2.0f, "%.2f");
   ImGui::DragFloat("Selection Switch Duration", &selectionSwitchDuration_, 0.01f, 0.01f, 1.0f, "%.2f s");
   ImGui::DragFloat("Selection Switch Scale", &selectionSwitchScale_, 0.01f, 0.0f, 1.0f, "%.2f");
   ImGui::Text("Resolved: %s", raceManager_ ? "true" : "false");
}
#endif

} // namespace App
