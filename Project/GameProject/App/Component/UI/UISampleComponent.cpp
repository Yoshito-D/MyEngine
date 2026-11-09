#include "GameEngine/pch.h"
#include "GameProject/App/Component/UI/UISampleComponent.h"
#include "GameEngine/UI/UIComponents.h"
#include "GameEngine/UI/UISystem.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Object/Component/UI/UITextComponent.h"
#include "GameEngine/Object/Component/Base/ComponentRegistry.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Audio/Audio.h"
#include <format>

namespace {
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   App::UISampleComponent::kTypeName,
   [](GameEngine::Object& object) -> GameEngine::IObjectComponent* {
      return object.AddComponent<App::UISampleComponent>();
   },
   App::UISampleComponent::kDisplayName, GameEngine::ToObjectTypeMask(GameEngine::ObjectType::Generic));
constexpr const char* kMainScreen = "ui_sample_main";
constexpr const char* kOverlayScreen = "ui_sample_overlay";
}

namespace App {
GameEngine::ObjectUpdateDomain UISampleComponent::GetUpdateDomain() const {
   return uiClock_ ? GameEngine::ObjectUpdateDomain::UI : GameEngine::ObjectUpdateDomain::Gameplay;
}

void UISampleComponent::PlayUIAudio() {
   if (auto* audio = GameEngine::EngineContext::GetAudio()) {
      audio->PlayOneShot(
         GameEngine::EngineContext::GetSoundClip("game/sounds/se/system/decision.mp3"), GameEngine::AudioBus::Ui);
   }
}

void UISampleComponent::OnSceneLoaded(GameEngine::SceneWorld& world) {
   world_ = &world;
   elapsed_ = 0.0f;
   if (uiClock_) return;
   auto& ui = world.GetUI();
   if (auto* object = ui.FindElement(kMainScreen, "open")) {
      object->GetComponent<GameEngine::UIButtonComponent>()->SetOnClick([this]() {
         PlayUIAudio();
         world_->GetUI().Push(kOverlayScreen);
      });
   }
   if (auto* object = ui.FindElement(kOverlayScreen, "close")) {
      object->GetComponent<GameEngine::UIButtonComponent>()->SetOnClick([this]() {
         PlayUIAudio();
         world_->GetUI().Pop();
      });
   }
   if (auto* object = ui.FindElement(kMainScreen, "slider")) {
      object->GetComponent<GameEngine::UISliderComponent>()->SetOnValueChanged([this](float value) {
         if (auto* label = world_->GetUI().FindElement(kMainScreen, "sliderValue")) {
            label->GetComponent<GameEngine::UITextComponent>()->SetText(std::format("Value: {:.2f}", value));
         }
         PlayUIAudio();
      });
   }
   if (auto* object = ui.FindElement(kMainScreen, "toggle")) {
      object->GetComponent<GameEngine::UIToggleComponent>()->SetOnValueChanged([this](bool value) {
         if (auto* label = world_->GetUI().FindElement(kMainScreen, "toggleValue")) {
            label->GetComponent<GameEngine::UITextComponent>()->SetText(value ? "Toggle: ON" : "Toggle: OFF");
         }
         PlayUIAudio();
      });
   }
}

void UISampleComponent::Update(float deltaTime) {
   if (!world_) return;
   elapsed_ += deltaTime;
   if (auto* label = world_->GetUI().FindElement(kMainScreen, uiClock_ ? "uiTime" : "gameTime")) {
      label->GetComponent<GameEngine::UITextComponent>()->SetText(
         std::format("{} time: {:.2f} s", uiClock_ ? "UI" : "Game", elapsed_));
   }
}

nlohmann::json UISampleComponent::Serialize() const {
   return {{"uiClock", uiClock_}};
}

void UISampleComponent::Deserialize(const nlohmann::json& data) {
   uiClock_ = data.value("uiClock", false);
   elapsed_ = 0.0f;
}
}
