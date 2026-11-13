#include "GameProject/App/Component/Title/TitleStartComponent.h"

#include "GameProject/App/Component/UI/MenuSelectionAnimationComponent.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Object/Component/Audio/AudioSourceComponent.h"
#include "GameEngine/Object/Component/UI/UITextComponent.h"
#include "GameEngine/Object/Object.h"
#include <Windows.h>
#include <cstddef>

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace App {

TitleStartComponent::TitleStartComponent() {
   ConfigureStates();
}

void TitleStartComponent::ConfigureStates() {
   stateMachine_.AddState(State::Main, {.onUpdate = [this](float) { UpdateMainMenu(); }});
   stateMachine_.AddState(State::StartSelection, {.onUpdate = [this](float) { UpdateStartSelection(); }});
   // 演出中にSelectで表示状態を戻さず、完了を確認してから次のメニューへ切り替える。
   stateMachine_.AddState(State::OpeningStartMenu, {.onUpdate = [this](float) {
      if (GetAnimation()->IsReactionComplete()) {
         stateMachine_.RequestState(State::StartSelection);
      }
   }});
   stateMachine_.AddState(State::Quitting, {.onUpdate = [this](float) {
      if (GetAnimation()->IsReactionComplete()) {
         stateMachine_.RequestState(State::Exiting);
      }
   }});
   stateMachine_.AddState(State::StartingScene, {.onEnter = [this]() {
      // 決定演出は演出コンポーネントが共通暗転中も更新する。
      const auto& scene = navigation_.GetSelectedIndex() == 0 ? tutorialScene_ : stageScene_;
      GameEngine::EngineContext::ChangeScene(scene);
   }});
   stateMachine_.AddState(State::Exiting, {.onEnter = [this]() { ExitApplication(); }});
   stateMachine_.SetTransitionRule(State::Main, {State::OpeningStartMenu, State::Quitting});
   stateMachine_.SetTransitionRule(State::StartSelection, {State::Main, State::StartingScene});
   stateMachine_.SetTransitionRule(State::OpeningStartMenu, {State::StartSelection});
   stateMachine_.SetTransitionRule(State::Quitting, {State::Exiting});
   stateMachine_.SetTransitionRule(State::StartingScene, {});
   stateMachine_.SetTransitionRule(State::Exiting, {});
   stateMachine_.SetTransitionCallback([this](State, State next) {
      // 初期Startでは表示を変更せず、メニュー間の移動時だけ表示演出を開始する。
      if (next == State::Main || next == State::StartSelection) {
         RevealMenu(*GetAnimation());
      }
   });
}

void TitleStartComponent::OnSceneLoaded(GameEngine::SceneWorld&) {
   stateMachine_.Stop();
   navigation_.Select(-1);
   navigation_.ResetInput();
   stateMachine_.Start(State::Main);
}

MenuSelectionAnimationComponent* TitleStartComponent::GetAnimation() {
   return HasOwner() ? GetOwner().GetComponent<MenuSelectionAnimationComponent>() : nullptr;
}

void TitleStartComponent::Update(float deltaTime) {
   auto* animation = GetAnimation();

   if (!animation || !animation->IsEnabled()) return;

   stateMachine_.Update(deltaTime);
   // 同じ決定入力を遷移先でも消費せず、入場処理だけをこのフレームに反映する。
   stateMachine_.ResolveRequests();
}

void TitleStartComponent::UpdateMainMenu() {
   auto* animation = GetAnimation();

   if (ReadMenuInput(*animation, false) != MenuInput::Confirm || !ConfirmSelection(*animation)) return;

   stateMachine_.RequestState(navigation_.GetSelectedIndex() == 0 ? State::OpeningStartMenu : State::Quitting);
}

void TitleStartComponent::UpdateStartSelection() {
   auto* animation = GetAnimation();
   const auto input = ReadMenuInput(*animation, true);

   if (input == MenuInput::Cancel) {
      if (auto* audioSource = GetOwner().GetComponent<GameEngine::AudioSourceComponent>()) {
         audioSource->PlayOneShot("cursor");
      }

      stateMachine_.RequestState(State::Main);
      return;
   }

   if (input != MenuInput::Confirm) return;

   const auto& selectedScene = navigation_.GetSelectedIndex() == 0 ? tutorialScene_ : stageScene_;

   // 空の遷移先や、演出対象を失った項目は決定できない状態として扱う。
   if (selectedScene.empty() || !ConfirmSelection(*animation)) return;

   stateMachine_.RequestState(State::StartingScene);
}

TitleStartComponent::MenuInput TitleStartComponent::ReadMenuInput(
   MenuSelectionAnimationComponent& animation, bool allowCancel) {
   // 表示される前の項目への決定や、押し続けによる次画面への入力持ち越しを防ぐ。
   if (animation.IsRevealing()) {
      return MenuInput::None;
   }

   // 初回の選択は全コンポーネントの参照解決後に行い、ロード時の通知順序に依存させない。
   if (navigation_.GetSelectedIndex() < 0 ||
       !animation.IsOptionAvailable(static_cast<std::size_t>(navigation_.GetSelectedIndex()))) {
      navigation_.Select(-1);

      // 上側の項目を優先しつつ、欠けている場合は利用可能な項目へフォールバックする。
      for (std::int32_t index = 0; index < 2; ++index) {
         if (animation.IsOptionAvailable(static_cast<std::size_t>(index))) {
            navigation_.Select(index);
            break;
         }
      }

      // 表示できる項目がないままシーン遷移だけを受け付けない。
      if (navigation_.GetSelectedIndex() < 0) {
         return MenuInput::None;
      }
   }

   auto* audioSource = GetOwner().GetComponent<GameEngine::AudioSourceComponent>();
   animation.Select(navigation_.GetSelectedIndex(), false);
   RefreshSelectionText(animation);
   const auto& cancelAction = GameEngine::EngineContext::GetInputActionState("UI", "UI.Cancel", 0);

   if (allowCancel && cancelAction.triggered) {
      return MenuInput::Cancel;
   }

   const auto& navigationAction = GameEngine::EngineContext::GetInputActionState("UI", "UI.Navigate", 0);
   const auto navigationDirection = navigation_.ReadDirection(navigationAction.value.x);
   if (navigation_.Move(animation, navigationDirection)) {
      animation.Select(navigation_.GetSelectedIndex());
      RefreshSelectionText(animation);

      if (audioSource) {
         audioSource->PlayOneShot("cursor");
      }
   }

   const auto& confirmAction = GameEngine::EngineContext::GetInputActionState("UI", "UI.Confirm", 0);
   return confirmAction.triggered ? MenuInput::Confirm : MenuInput::None;
}

bool TitleStartComponent::ConfirmSelection(MenuSelectionAnimationComponent& animation) {
   if (!animation.ConfirmSelection()) {
      return false;
   }

   if (auto* audioSource = GetOwner().GetComponent<GameEngine::AudioSourceComponent>()) {
      audioSource->PlayOneShot("decision");
   }

   return true;
}

void TitleStartComponent::ExitApplication() {
#ifdef USE_IMGUI
   // エディターではゲームの再生を終了し、編集中のシーンへ戻す。
   GameEngine::EngineContext::RequestPlayModeStop();
#else
   PostQuitMessage(0);
#endif
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

void TitleStartComponent::RevealMenu(MenuSelectionAnimationComponent& animation) {
   navigation_.Select(-1);
   animation.Hide();
   navigation_.Move(animation, 1);
   animation.Reveal(navigation_.GetSelectedIndex());
   RefreshSelectionText(animation);
}

void TitleStartComponent::RefreshSelectionText(MenuSelectionAnimationComponent& animation) {
   const bool mainMenu = stateMachine_.GetCurrentState() == State::Main;
   const char* labels[] = {mainMenu ? "はじめる" : "チュートリアルから", mainMenu ? "やめる" : "ステージから"};

   for (std::int32_t index = 0; index < 2; ++index) {
      if (auto* text = animation.GetOptionText(static_cast<std::size_t>(index))) {
         text->SetText(labels[index]);
         const float brightness = navigation_.GetSelectedIndex() == index ? 1.0f : 0.45f;
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
   const auto state = stateMachine_.GetCurrentState();
   const bool startMenu = state == State::StartSelection || state == State::StartingScene;
   const bool requested = state == State::StartingScene || state == State::Exiting;
   ImGui::Text("Menu: %s", startMenu ? "Start Selection" : "Main");
   ImGui::Text("Selected Option: %d", navigation_.GetSelectedIndex());
   ImGui::Text("Start Requested: %s", requested ? "true" : "false");
}
#endif

} // namespace App
