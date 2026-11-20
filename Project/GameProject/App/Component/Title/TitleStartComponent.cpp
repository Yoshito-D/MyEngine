#include "GameProject/App/Component/Title/TitleStartComponent.h"

#include "GameProject/App/Component/UI/MenuSelectionAnimationComponent.h"
#include "GameProject/App/Component/UI/PauseMenuComponent.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Object/Component/Audio/AudioSourceComponent.h"
#include "GameEngine/Object/Component/UI/UITextComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Scene/SceneWorld.h"
#include <Windows.h>
#include <cstddef>

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace App {
namespace {
constexpr const char* kMainMenuLabels[] = {"はじめる", "設定", "やめる"};
constexpr const char* kStartMenuLabels[] = {"チュートリアルから", "ステージから"};
}

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
   stateMachine_.AddState(State::OpeningSettings, {.onUpdate = [this](float) {
      if (GetAnimation()->IsReactionComplete()) {
         OpenSettings();
      }
   }});
   stateMachine_.AddState(State::Settings, {.onUpdate =
                                                 [this](float) {
                                                    const auto* menu = GetOwner().GetComponent<PauseMenuComponent>();

                                                    if (!menu || !menu->IsSettingsVisible()) {
                                                       stateMachine_.RequestState(State::Main);
                                                    }
                                                 },
                                              .onExit = [this]() { RestoreTitleText(); }});
   stateMachine_.AddState(State::Quitting, {.onUpdate = [this](float) {
      if (GetAnimation()->IsReactionComplete()) {
         stateMachine_.RequestState(State::Exiting);
      }
   }});
   stateMachine_.AddState(State::StartingScene, {.onUpdate = [this](float) {
      // 暗転後もタイトルが残っていれば、表示が戻ってから選択を再開する。
      if (!GameEngine::EngineContext::IsSceneTransitioning()) {
         stateMachine_.RequestState(State::StartSelection);
      }
   }});
   stateMachine_.AddState(State::Exiting, {.onEnter = [this]() { ExitApplication(); }});
   stateMachine_.SetTransitionRule(State::Main, {State::OpeningStartMenu, State::OpeningSettings, State::Quitting});
   stateMachine_.SetTransitionRule(State::StartSelection, {State::Main, State::StartingScene});
   stateMachine_.SetTransitionRule(State::OpeningStartMenu, {State::StartSelection});
   stateMachine_.SetTransitionRule(State::OpeningSettings, {State::Settings, State::Main});
   stateMachine_.SetTransitionRule(State::Settings, {State::Main});
   stateMachine_.SetTransitionRule(State::Quitting, {State::Exiting});
   stateMachine_.SetTransitionRule(State::StartingScene, {State::StartSelection});
   stateMachine_.SetTransitionRule(State::Exiting, {});
   stateMachine_.SetTransitionCallback([this](State previous, State next) {
      // 初期Startでは表示を変更せず、メニュー間の移動時だけ表示演出を開始する。
      if (next == State::Main || next == State::StartSelection) {
         if (previous != State::Settings && previous != State::OpeningSettings) {
            navigation_.Select(-1);
         }

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

void TitleStartComponent::OnDetach() {
   RestoreTitleText();
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
   constexpr State kActions[] = {State::OpeningStartMenu, State::OpeningSettings, State::Quitting};
   static_assert(std::size(kActions) == std::size(kMainMenuLabels));
   auto* animation = GetAnimation();

   if (ReadMenuInput(*animation) != MenuInput::Confirm || !ConfirmSelection(*animation)) return;

   stateMachine_.RequestState(kActions[navigation_.GetSelectedIndex()]);
}

void TitleStartComponent::UpdateStartSelection() {
   auto* animation = GetAnimation();
   const auto input = ReadMenuInput(*animation);

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

   if (GameEngine::EngineContext::ChangeScene(selectedScene)) {
      stateMachine_.RequestState(State::StartingScene);
   } else {
      RevealMenu(*animation);
   }
}

void TitleStartComponent::OpenSettings() {
   auto* menu = GetOwner().GetComponent<PauseMenuComponent>();

   if (!menu || !menu->OpenSettings()) {
      stateMachine_.RequestState(State::Main);
      return;
   }

   // 画面が重なる間はタイトル名を隠し、閉じたときに元の不透明度を戻す。
   if (auto* text = GetTitleText()) {
      titleOpacity_ = text->GetStyle().color.w;
      text->SetOpacity(0.0f);
   }

   stateMachine_.RequestState(State::Settings);
}

GameEngine::UITextComponent* TitleStartComponent::GetTitleText() {
   auto* world = GetOwner().GetWorld();
   auto* object = world ? world->FindObjectById(titleTextId_) : nullptr;
   return object ? object->GetComponent<GameEngine::UITextComponent>() : nullptr;
}

void TitleStartComponent::RestoreTitleText() {
   if (!titleOpacity_) return;

   if (auto* text = GetTitleText()) {
      text->SetOpacity(*titleOpacity_);
   }

   titleOpacity_.reset();
}

TitleStartComponent::MenuInput TitleStartComponent::ReadMenuInput(MenuSelectionAnimationComponent& animation) {
   // 表示される前の項目への決定や、押し続けによる次画面への入力持ち越しを防ぐ。
   if (animation.IsRevealing()) {
      return MenuInput::None;
   }

   const auto optionCount = GetMenuLabels().size();

   // 初回の選択は全コンポーネントの参照解決後に行い、ロード時の通知順序に依存させない。
   if (navigation_.GetSelectedIndex() < 0 ||
       !animation.IsOptionAvailable(static_cast<std::size_t>(navigation_.GetSelectedIndex()))) {
      navigation_.Select(-1);
      navigation_.Move(animation, 1, optionCount);

      // 表示できる項目がないままシーン遷移だけを受け付けない。
      if (navigation_.GetSelectedIndex() < 0) {
         return MenuInput::None;
      }
   }

   auto* audioSource = GetOwner().GetComponent<GameEngine::AudioSourceComponent>();
   animation.Select(navigation_.GetSelectedIndex(), false);
   RefreshSelectionText(animation);
   const auto& cancelAction = GameEngine::EngineContext::GetInputActionState("UI", "UI.Cancel", 0);

   if (stateMachine_.GetCurrentState() == State::StartSelection && cancelAction.triggered) {
      return MenuInput::Cancel;
   }

   const auto& navigationAction = GameEngine::EngineContext::GetInputActionState("UI", "UI.Navigate", 0);
   const auto navigationDirection = navigation_.ReadDirection(navigationAction.value.x);
   if (navigation_.Move(animation, navigationDirection, optionCount)) {
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
   return {{"tutorialScene", tutorialScene_}, {"stageScene", stageScene_}, {"titleTextId", titleTextId_}};
}

void TitleStartComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) return;

   if (data.contains("tutorialScene") && data.at("tutorialScene").is_string()) {
      tutorialScene_ = data.at("tutorialScene").get<std::string>();
   }

   if (data.contains("stageScene") && data.at("stageScene").is_string()) {
      stageScene_ = data.at("stageScene").get<std::string>();
   }

   if (data.contains("titleTextId") && data.at("titleTextId").is_string()) {
      titleTextId_ = data.at("titleTextId").get<std::string>();
   }
}

void TitleStartComponent::RevealMenu(MenuSelectionAnimationComponent& animation) {
   animation.Hide();

   if (!animation.Reveal(navigation_.GetSelectedIndex())) {
      navigation_.Select(-1);
      navigation_.Move(animation, 1, GetMenuLabels().size());
      animation.Reveal(navigation_.GetSelectedIndex());
   }

   RefreshSelectionText(animation);
}

std::span<const char* const> TitleStartComponent::GetMenuLabels() const {
   if (stateMachine_.GetCurrentState() == State::StartSelection) {
      return kStartMenuLabels;
   }

   return kMainMenuLabels;
}

void TitleStartComponent::RefreshSelectionText(MenuSelectionAnimationComponent& animation) {
   const auto labels = GetMenuLabels();

   for (std::size_t index = 0; index < labels.size(); ++index) {
      if (auto* text = animation.GetOptionText(index)) {
         text->SetText(labels[index]);
         const float brightness = navigation_.GetSelectedIndex() == static_cast<std::int32_t>(index) ? 1.0f : 0.45f;
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
   GameEngine::EditorUI::ObjectReference("Title Text", titleTextId_, "UITextComponent");
   const auto state = stateMachine_.GetCurrentState();
   const bool startMenu = state == State::StartSelection || state == State::StartingScene;
   const bool requested = state == State::StartingScene || state == State::Exiting;
   const bool settings = state == State::OpeningSettings || state == State::Settings;
   ImGui::Text("Menu: %s", startMenu ? "Start Selection" : (settings ? "Settings" : "Main"));
   ImGui::Text("Selected Option: %d", navigation_.GetSelectedIndex());
   ImGui::Text("Start Requested: %s", requested ? "true" : "false");
}
#endif

} // namespace App
