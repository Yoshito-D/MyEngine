#include "GameProject/App/Component/Race/UI/RaceResultUIComponent.h"

#include "GameProject/App/Component/Race/RaceManagerComponent.h"
#include "GameProject/App/Component/Race/UI/RaceTimeFormatting.h"
#include "GameProject/App/Component/UI/MenuSelectionAnimationComponent.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Object/Component/UI/UITextComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Scene/SceneWorld.h"
#include <cstddef>
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

   // 実行時ロードではOnReferencesChangedが自動では呼ばれないため、ここからも参照を解決する。
   OnReferencesChanged(sceneWorld);
}

void RaceResultUIComponent::OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) {
   // シーン再読み込み後に破棄済みオブジェクトを参照しないよう、保存済みIDから解決し直す。
   raceManager_ = nullptr;

   if (auto* managerObject = sceneWorld.FindObjectById(raceManagerId_)) {
      raceManager_ = managerObject->GetComponent<RaceManagerComponent>();
   }
}

void RaceResultUIComponent::Update(float) {
   if (!raceManager_ || !HasOwner()) return;

   auto* text = GetOwner().GetComponent<GameEngine::UITextComponent>();
   auto* animation = GetOwner().GetComponent<MenuSelectionAnimationComponent>();

   if (!text || !animation || !animation->IsEnabled()) return;

   // このUITextはレース中もシーンに残るため、終了状態以外では内容を消しておく。
   // これによりリスタート後に前回のリザルトが一瞬残ることも防ぐ。
   if (raceManager_->GetState() != RaceManagerComponent::State::Finished) {
      text->SetText("");
      showingResult_ = false;
      resultRequested_ = false;
      selectedOption_ = 1;
      navigationLatched_ = false;
      animation->Hide();
      return;
   }

   if (!showingResult_) {
      showingResult_ = true;
      resultRequested_ = false;
      selectedOption_ = animation->IsOptionAvailable(1) ? 1 : 0;
      navigationLatched_ = false;
      animation->Select(selectedOption_, false);
      RefreshSelectionText(*animation);
   }

   text->SetText(BuildResultText());

   // 決定後の拡大・フェードは演出コンポーネントだけが更新する。
   if (resultRequested_) return;

   animation->Select(selectedOption_, false);
   RefreshSelectionText(*animation);
   const auto& navigationAction = GameEngine::EngineContext::GetInputActionState("UI", "UI.NavigateHorizontal", 0);
   const std::int32_t navigationDirection =
      navigationAction.value.x > 0.5f ? 1 : (navigationAction.value.x < -0.5f ? -1 : 0);

   // 閾値未満を中立として扱い、スティックの微小な揺れで選択が変わらないようにする。
   if (navigationDirection == 0) {
      navigationLatched_ = false;
   } else if (!navigationLatched_) {
      if (MoveSelection(*animation, navigationDirection)) {
         animation->Select(selectedOption_);
         RefreshSelectionText(*animation);
      }

      navigationLatched_ = true;
   }

   const auto& confirmAction = GameEngine::EngineContext::GetInputActionState("UI", "UI.Confirm", 0);

   if (confirmAction.triggered) {
      ConfirmSelection(*animation);
   }
}

nlohmann::json RaceResultUIComponent::Serialize() const {
   return nlohmann::json{{"raceManagerId", raceManagerId_}, {"titleScene", titleScene_}};
}

void RaceResultUIComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) return;

   if (data.contains("raceManagerId") && data.at("raceManagerId").is_string()) {
      raceManagerId_ = data.at("raceManagerId").get<std::string>();
   }

   if (data.contains("titleScene") && data.at("titleScene").is_string()) {
      titleScene_ = data.at("titleScene").get<std::string>();
   }
}

bool RaceResultUIComponent::MoveSelection(MenuSelectionAnimationComponent& animation, std::int32_t direction) {
   constexpr std::int32_t kOptionCount = 2;
   const std::int32_t previousSelection = selectedOption_;
   std::int32_t candidate = selectedOption_;

   // 端では巡回し、未解決の項目を飛ばす。候補数までの試行に制限して全欠落時も終了させる。
   for (std::int32_t attempt = 0; attempt < kOptionCount; ++attempt) {
      candidate = (candidate + direction + kOptionCount) % kOptionCount;

      if (animation.IsOptionAvailable(static_cast<std::size_t>(candidate))) {
         selectedOption_ = candidate;
         return selectedOption_ != previousSelection;
      }
   }

   return false;
}

void RaceResultUIComponent::RefreshSelectionText(MenuSelectionAnimationComponent& animation) {
   if (auto* text = animation.GetOptionText(0)) {
      text->SetText(std::string(selectedOption_ == 0 ? "> " : "  ") + "TITLE");
   }

   if (auto* text = animation.GetOptionText(1)) {
      text->SetText(std::string(selectedOption_ == 1 ? "> " : "  ") + "RESTART");
   }
}

bool RaceResultUIComponent::ConfirmSelection(MenuSelectionAnimationComponent& animation) {
   if (!animation.IsOptionAvailable(static_cast<std::size_t>(selectedOption_))) {
      return false;
   }

   if (selectedOption_ == 0) {
      if (titleScene_.empty()) {
         return false;
      }

      GameEngine::EngineContext::ChangeScene(titleScene_);
   } else if (!raceManager_->RequestRestart()) {
      return false;
   }

   resultRequested_ = animation.ConfirmSelection();
   return resultRequested_;
}

std::string RaceResultUIComponent::BuildResultText() const {
   std::ostringstream stream;
   stream << "FINISH\nTIME " << FormatRaceTime(raceManager_->GetElapsedTime()) << "\n\nBEST TIMES\n";

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

   return stream.str();
}

#ifdef USE_IMGUI
void RaceResultUIComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!ImGui::CollapsingHeader(header.c_str())) return;

   GameEngine::EditorUI::ObjectReference("Race Manager", raceManagerId_, "RaceManagerComponent");
   GameEngine::EditorUI::SceneReference("Title Scene", titleScene_);
   ImGui::Text("Resolved: %s", raceManager_ ? "true" : "false");
}
#endif

} // namespace App
