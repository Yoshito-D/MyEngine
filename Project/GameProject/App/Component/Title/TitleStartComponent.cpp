#include "GameProject/App/Component/Title/TitleStartComponent.h"

#include "GameProject/App/Component/UI/MenuSelectionAnimationComponent.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Object/Component/UI/UITextComponent.h"
#include "GameEngine/Object/Object.h"
#include <Windows.h>
#include <cstddef>

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace App {

void TitleStartComponent::OnSceneLoaded(GameEngine::SceneWorld&) {
   menuPage_ = MenuPage::Main;
   pendingAction_ = PendingAction::None;
   selectedOption_ = -1;
   startRequested_ = false;
   navigationLatched_ = false;
}

void TitleStartComponent::Update(float) {
   if (!HasOwner() || startRequested_) return;

   auto* animation = GetOwner().GetComponent<MenuSelectionAnimationComponent>();

   if (!animation || !animation->IsEnabled()) return;

   // 演出中にSelectで表示状態を戻さず、完了を確認してから次のメニューへ切り替える。
   if (pendingAction_ != PendingAction::None) {
      if (!animation->IsReactionComplete()) return;

      const auto action = pendingAction_;
      pendingAction_ = PendingAction::None;

      if (action == PendingAction::OpenStartMenu) {
         SwitchMenu(*animation, MenuPage::StartSelection);
      } else {
         startRequested_ = true;
#ifdef USE_IMGUI
         // エディターではゲームの再生を終了し、編集中のシーンへ戻す。
         GameEngine::EngineContext::RequestPlayModeStop();
#else
         PostQuitMessage(0);
#endif
      }

      return;
   }

   // 表示される前の項目への決定や、押し続けによる次画面への入力持ち越しを防ぐ。
   if (animation->IsRevealing()) return;

   // 初回の選択は全コンポーネントの参照解決後に行い、ロード時の通知順序に依存させない。
   if (selectedOption_ < 0 || !animation->IsOptionAvailable(static_cast<std::size_t>(selectedOption_))) {
      selectedOption_ = -1;

      // 上側の項目を優先しつつ、欠けている場合は利用可能な項目へフォールバックする。
      for (std::int32_t index = 0; index < 2; ++index) {
         if (animation->IsOptionAvailable(static_cast<std::size_t>(index))) {
            selectedOption_ = index;
            break;
         }
      }

      // 表示できる項目がないままシーン遷移だけを受け付けない。
      if (selectedOption_ < 0) return;
   }

   animation->Select(selectedOption_, false);
   RefreshSelectionText(*animation);
   const auto& cancelAction = GameEngine::EngineContext::GetInputActionState("UI", "UI.Cancel", 0);

   if (menuPage_ == MenuPage::StartSelection && cancelAction.triggered) {
      SwitchMenu(*animation, MenuPage::Main);
      return;
   }

   const auto& navigationAction = GameEngine::EngineContext::GetInputActionState("UI", "UI.Navigate", 0);
   const std::int32_t navigationDirection =
      navigationAction.value.x > 0.5f ? 1 : (navigationAction.value.x < -0.5f ? -1 : 0);

   // 閾値未満を中立として扱い、スティックの微小な揺れで選択が変わらないようにする。
   if (navigationDirection == 0) {
      navigationLatched_ = false;
   } else if (!navigationLatched_) {
      // 押し続けで選択が往復しないよう、軸が中立へ戻るまで次の移動を受け付けない。
      if (MoveSelection(*animation, navigationDirection)) {
         animation->Select(selectedOption_);
         RefreshSelectionText(*animation);
      }

      navigationLatched_ = true;
   }

   const auto& confirmAction = GameEngine::EngineContext::GetInputActionState("UI", "UI.Confirm", 0);

   if (!confirmAction.triggered) return;

   if (menuPage_ == MenuPage::Main) {
      if (animation->ConfirmSelection()) {
         pendingAction_ = selectedOption_ == 0 ? PendingAction::OpenStartMenu : PendingAction::Quit;
      }

      return;
   }

   const std::string& selectedScene = selectedOption_ == 0 ? tutorialScene_ : stageScene_;

   // 空の遷移先や、演出対象を失った項目は決定できない状態として扱う。
   if (selectedScene.empty() || !animation->ConfirmSelection()) return;

   // 決定演出は演出コンポーネントが共通暗転中も更新する。
   startRequested_ = true;
   GameEngine::EngineContext::ChangeScene(selectedScene);
}

nlohmann::json TitleStartComponent::Serialize() const {
   return nlohmann::json{{"tutorialScene", tutorialScene_}, {"stageScene", stageScene_}};
}

void TitleStartComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) return;

   if (data.contains("tutorialScene") && data.at("tutorialScene").is_string()) {
      tutorialScene_ = data.at("tutorialScene").get<std::string>();
   }

   if (data.contains("stageScene") && data.at("stageScene").is_string()) {
      stageScene_ = data.at("stageScene").get<std::string>();
   }
}

void TitleStartComponent::SwitchMenu(MenuSelectionAnimationComponent& animation, MenuPage page) {
   menuPage_ = page;
   selectedOption_ = -1;
   animation.Hide();
   MoveSelection(animation, 1);
   animation.Reveal(selectedOption_);
   RefreshSelectionText(animation);
}

bool TitleStartComponent::MoveSelection(MenuSelectionAnimationComponent& animation, std::int32_t direction) {
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

void TitleStartComponent::RefreshSelectionText(MenuSelectionAnimationComponent& animation) {
   const char* labels[] = {menuPage_ == MenuPage::Main ? "はじめる" : "チュートリアルから",
      menuPage_ == MenuPage::Main ? "やめる" : "ステージから"};

   for (std::int32_t index = 0; index < 2; ++index) {
      if (auto* text = animation.GetOptionText(static_cast<std::size_t>(index))) {
         text->SetText(labels[index]);
         const float brightness = selectedOption_ == index ? 1.0f : 0.45f;
         auto color = text->GetStyle().color;
         color.x = brightness;
         color.y = brightness;
         color.z = brightness;
         text->SetColor(color);
      }
   }
}

#ifdef USE_IMGUI
void TitleStartComponent::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!ImGui::CollapsingHeader(header.c_str())) return;

   GameEngine::EditorUI::SceneReference("Tutorial Scene", tutorialScene_);
   GameEngine::EditorUI::SceneReference("Stage Scene", stageScene_);
   ImGui::Text("Menu: %s", menuPage_ == MenuPage::Main ? "Main" : "Start Selection");
   ImGui::Text("Selected Option: %d", selectedOption_);
   ImGui::Text("Start Requested: %s", startRequested_ ? "true" : "false");
}
#endif

} // namespace App
