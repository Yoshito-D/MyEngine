#include "GameProject/App/Component/UI/PauseMenuComponent.h"
#include "GameProject/App/Component/Race/RaceManagerComponent.h"
#include "GameEngine/Audio/Audio.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Object/Component/UI/UITextComponent.h"
#include "GameEngine/Object/Component/Rendering/MaterialComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Scene/SceneManager.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/UI/UIComponents.h"
#include "GameEngine/UI/UISystem.h"
#include "GameEngine/Utility/JsonFile.h"
#include "GameEngine/Utility/Logger.h"
#include <format>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace {
/// @brief タイム記録と同じ保存フォルダーに置く、ユーザー音量設定の保存先。
constexpr const char* kAudioSettingsFile = "Saved/audio_settings.json";
constexpr std::int32_t kAudioSettingsVersion = 1;
constexpr const char* kPauseScreenAsset = "game/ui/PauseMenu.json";
constexpr const char* kSettingsScreenAsset = "game/ui/PauseSettings.json";
constexpr const char* kCursorSoundAsset = "game/sounds/se/system/cursor.mp3";
constexpr const char* kDecisionSoundAsset = "game/sounds/se/system/decision.mp3";
constexpr float kVolumePercent = 100.0f;
constexpr float kPausedBgmGain = 0.25f;
constexpr float kPauseBackgroundBlur = 1.0f;
constexpr float kSelectionFrameLoopSeconds = 3.0f;

float ReadVolume(const nlohmann::json& settings, const char* key) {
   const auto& value = settings.at(key);

   if (!value.is_number()) {
      throw std::invalid_argument(std::string("volume is not a number: ") + key);
   }

   const float volume = value.get<float>();

   if (!std::isfinite(volume) || volume < 0.0f || volume > 1.0f) {
      throw std::invalid_argument(std::string("volume is outside 0..1: ") + key);
   }

   return volume;
}

void SetSoundEffectVolume(GameEngine::Audio& audio, float volume) {
   // メニュー操作音はポーズ中も再生できるUIバスを使い、音量はゲーム内SEと共有する。
   audio.SetBusVolume(GameEngine::AudioBus::Se, volume);
   audio.SetBusVolume(GameEngine::AudioBus::Ui, volume);
}
}

namespace App {
void PauseMenuComponent::LoadAudioSettings(GameEngine::Audio& audio) {
   const std::filesystem::path file(kAudioSettingsFile);

   try {
      // 初回起動では既定の音量を使い、設定を変更するまでは保存ファイルを作らない。
      if (!std::filesystem::exists(file)) return;

      std::ifstream input(file);

      if (!input.is_open()) {
         throw std::runtime_error("could not open settings file");
      }

      nlohmann::json settings;
      input >> settings;

      if (!settings.is_object() || !settings.contains("version") || !settings.at("version").is_number_integer() ||
          settings.at("version") != kAudioSettingsVersion) {
         throw std::invalid_argument("invalid settings format");
      }

      // 全項目を検証してから適用し、一部だけ読み込まれた音量で起動することを防ぐ。
      const float master = ReadVolume(settings, "master");
      const float bgm = ReadVolume(settings, "bgm");
      const float se = ReadVolume(settings, "se");
      audio.SetMasterVolume(master);
      audio.SetBusVolume(GameEngine::AudioBus::Bgm, bgm);
      SetSoundEffectVolume(audio, se);
   } catch (const std::exception& exception) {
      Logger::Fail("Audio settings could not be loaded: " + file.generic_string() + ": " + exception.what(),
         Logger::LogChannel::Game);
   }
}

bool PauseMenuComponent::SaveAudioSettings(const GameEngine::Audio& audio) {
   const std::filesystem::path file(kAudioSettingsFile);
   const nlohmann::json settings{{"version", kAudioSettingsVersion}, {"master", audio.GetMasterVolume()},
      {"bgm", audio.GetBusVolume(GameEngine::AudioBus::Bgm)}, {"se", audio.GetBusVolume(GameEngine::AudioBus::Se)}};

   if (GameEngine::SaveJsonFileAtomically(file, settings)) {
      return true;
   }

   Logger::Error("Audio settings could not be saved: " + file.generic_string(), Logger::LogChannel::Game);
   return false;
}

void PauseMenuComponent::SavePendingAudioSettings() {
   if (!audioSettingsDirty_) return;

   if (const auto* audio = GameEngine::EngineContext::GetAudio()) {
      SaveAudioSettings(*audio);
   } else {
      Logger::Error("Cannot save audio settings: audio service is unavailable", Logger::LogChannel::Game);
   }

   audioSettingsDirty_ = false;
}

void PauseMenuComponent::OnSceneLoaded(GameEngine::SceneWorld& world) {
   world_ = &world;
   pauseScreenId_.clear();
   settingsScreenId_.clear();
   selectionScreenId_.clear();
   selectedElementId_.clear();
}

void PauseMenuComponent::Update(float deltaTime) {
   if (!world_) return;

   auto& ui = world_->GetUI();
   const auto* pauseScreen = world_->FindObjectById(pauseScreenId_);
   const bool showingSettings = IsSettingsVisible();

   if (!showingSettings) {
      SavePendingAudioSettings();
   }

   const bool showingPause = (pauseScreen && ui.IsUpdateVisible(*pauseScreen)) || showingSettings;

   // 背景と枠は下の画面に残し、設定の表示中はメニューの中身だけを隠す。
   SetMenuContentVisible(!showingSettings);

   if (showingSettings) {
      UpdateSelectionVisuals(
         settingsScreenId_, {"master", "bgm", "se", "back"}, deltaTime);
   } else if (showingPause) {
      UpdateSelectionVisuals(
         pauseScreenId_, {"restart", "title", "settings", "resume"}, deltaTime);
   }

   // ポーズ固有の適用条件と演出値はゲーム側で決め、エンジンへは汎用の描画・音量操作だけを渡す。
   GameEngine::EngineContext::SetOverlayBackgroundBlur(showingPause ? kPauseBackgroundBlur : 0.0f);
   presentationApplied_ = showingPause;

   if (settingsOnly_) {
      UpdateSelectionSound(showingSettings ? settingsScreenId_ : std::string{});
      return;
   }

   if (auto* audio = GameEngine::EngineContext::GetAudio()) {
      audio->SetBgmDucking(showingPause ? kPausedBgmGain : 1.0f);
   }

   if (IsPresentationActive()) return;

   UpdateSelectionSound(showingSettings ? settingsScreenId_ : (showingPause ? pauseScreenId_ : std::string{}));

   // 閉じる演出中と入力解放待ちにも新たな画面を重ねず、再開入力の持ち越しを防ぐ。
   if (ui.BlocksGameplayInput() ||
       !GameEngine::EngineContext::GetInputActionState("Gameplay", "Gameplay.Pause").triggered) {
      return;
   }

   // 編集用スナップショットへ実行時画面を混入させないよう、初回操作時にだけ生成する。
   if (pauseScreenId_.empty() && !CreatePauseScreen()) return;

   SetDefaultFocus("restart");

   if (ui.Push(pauseScreenId_)) {
      PlayUISound(kDecisionSoundAsset);
   }
}

bool PauseMenuComponent::IsPresentationActive() const {
   if (GameEngine::EngineContext::IsSceneTransitioning()) {
      return true;
   }

   // 導入カメラの移動と明暗転はシーン管理とは別に進むため、ゲーム側の進行状態も確認する。
   for (const auto* object : world_->GetObjectStore().GetObjects()) {
      const auto* race = object->GetComponent<RaceManagerComponent>();

      if (race && race->IsEnabled() && race->GetState() == RaceManagerComponent::State::Intro) {
         return true;
      }
   }

   return false;
}

bool PauseMenuComponent::CreatePauseScreen() {
   auto& ui = world_->GetUI();
   pauseScreenId_ = ui.InstantiateScreen(kPauseScreenAsset);

   if (pauseScreenId_.empty()) {
      Logger::Error("Pause menu could not be created: " + ui.GetLastError());
      return false;
   }

   BindButton(pauseScreenId_, "restart", [this]() { RequestSceneChange(true); });
   BindButton(pauseScreenId_, "title", [this]() { RequestSceneChange(false); });
   BindButton(pauseScreenId_, "settings", [this]() {
      if (OpenSettings()) {
         PlayUISound(kDecisionSoundAsset);
      }
   });
   BindButton(pauseScreenId_, "resume", [this]() { CloseTopScreen(); });
   return true;
}

void PauseMenuComponent::BindButton(const std::string& rootId, const char* key, std::function<void()> callback) {
   auto* object = world_->GetUI().FindElement(rootId, key);
   auto* button = object ? object->GetComponent<GameEngine::UIButtonComponent>() : nullptr;

   if (!button) {
      Logger::Fail("Missing button " + std::string(key) + " in screen " + rootId, Logger::LogChannel::Game);
   }

   button->SetOnClick(std::move(callback));
}

bool PauseMenuComponent::IsSettingsVisible() const {
   const auto* screen = world_ ? world_->FindObjectById(settingsScreenId_) : nullptr;
   return screen && world_->GetUI().IsUpdateVisible(*screen);
}

bool PauseMenuComponent::OpenSettings() {
   if (!world_ || !IsEnabled() || GameEngine::EngineContext::IsSceneTransitioning()) {
      return false;
   }

   auto& ui = world_->GetUI();

   if (settingsScreenId_.empty()) {
      settingsScreenId_ = ui.InstantiateScreen(kSettingsScreenAsset);

      if (settingsScreenId_.empty()) {
         Logger::Error("Audio settings could not be created: " + ui.GetLastError());
         return false;
      }

      BindButton(settingsScreenId_, "back", [this]() { CloseTopScreen(); });

      if (settingsOnly_) {
         auto* screen = world_->FindObjectById(settingsScreenId_)->GetComponent<GameEngine::UIScreenComponent>();
         auto settings = screen->DescribeSettings();
         settings.pausesGameplay = false;
         screen->Configure(settings);
      }
   }

   // 音量はシーンより長く生存するAudioが保持し、リスタートやタイトル遷移後も引き継ぐ。
   if (auto* audio = GameEngine::EngineContext::GetAudio()) {
      BindVolumeSlider("master", audio->GetMasterVolume(), [audio](float value) { audio->SetMasterVolume(value); });
      BindVolumeSlider("bgm", audio->GetBusVolume(GameEngine::AudioBus::Bgm),
         [audio](float value) { audio->SetBusVolume(GameEngine::AudioBus::Bgm, value); });
      BindVolumeSlider("se", audio->GetBusVolume(GameEngine::AudioBus::Se),
         [audio](float value) { SetSoundEffectVolume(*audio, value); });
   }

   return ui.Push(settingsScreenId_);
}

void PauseMenuComponent::BindVolumeSlider(const char* key, float volume, std::function<void(float)> callback) {
   auto* object = world_->GetUI().FindElement(settingsScreenId_, key);
   auto* slider = object ? object->GetComponent<GameEngine::UISliderComponent>() : nullptr;

   if (!slider) {
      Logger::Fail("Missing audio slider " + std::string(key) + " in screen " + settingsScreenId_,
         Logger::LogChannel::Game);
   }

   slider->SetValue(volume, false);
   UpdateVolumeText(key, slider->GetValue());
   slider->SetOnValueChanged([this, key = std::string(key), callback = std::move(callback)](float value) {
      callback(value);

      if (UpdateVolumeText(key, value)) {
         PlayUISound(kCursorSoundAsset);
         // 選択と音量変更が同じフレームに起きても、カーソル音を二重に鳴らさない。
         selectionScreenId_ = settingsScreenId_;
         selectedElementId_ = world_->GetUI().GetFocusedElementId(settingsScreenId_);
      }

      // 連続操作は即座に試聴できるよう適用し、保存は画面を閉じたときにまとめる。
      audioSettingsDirty_ = true;
   });
}

bool PauseMenuComponent::UpdateVolumeText(const std::string& key, float volume) {
   if (auto* object = world_->GetUI().FindElement(settingsScreenId_, key + "Value")) {
      if (auto* text = object->GetComponent<GameEngine::UITextComponent>()) {
         const auto label = std::format("{:.0f}%", volume * kVolumePercent);

         if (text->GetText() == label) {
            return false;
         }

         text->SetText(label);
         return true;
      }
   }

   return false;
}

void PauseMenuComponent::CloseTopScreen() {
   if (!world_ || GameEngine::EngineContext::IsSceneTransitioning()) return;

   PlayUISound(kDecisionSoundAsset);
   world_->GetUI().Pop();
}

void PauseMenuComponent::RequestSceneChange(bool restart) {
   auto* manager = GameEngine::EngineContext::GetSceneManager();

   if (!manager || manager->IsSceneTransitioning()) return;

   // シーンの切替完了までポーズ画面を残し、暗転中にゲームが再開することを防ぐ。
   const auto request = restart ? manager->RequestReload() : manager->RequestSceneChange(titleScene_);

   if (request == GameEngine::SceneRequestStatus::Accepted) {
      PlayUISound(kDecisionSoundAsset);
   }
}

void PauseMenuComponent::SetMenuContentVisible(bool visible) {
   if (auto* object = world_->GetUI().FindElement(pauseScreenId_, "content")) {
      if (auto* group = object->GetComponent<GameEngine::UIGroupComponent>()) {
         auto settings = group->DescribeSettings();

         if (settings.visible != visible) {
            settings.visible = visible;
            group->Configure(settings);

            // 設定から戻って選択候補が復活したときは、設定ボタンを初期選択先にする。
            if (visible) {
               SetDefaultFocus("settings");
            }
         }
      }
   }
}

void PauseMenuComponent::SetDefaultFocus(const char* key) {
   auto* root = world_->FindObjectById(pauseScreenId_);
   auto* screen = root ? root->GetComponent<GameEngine::UIScreenComponent>() : nullptr;

   if (!screen || screen->DescribeSettings().defaultFocusKey == key) return;

   auto settings = screen->DescribeSettings();
   settings.defaultFocusKey = key;
   screen->Configure(settings);
}

void PauseMenuComponent::UpdateSelectionSound(const std::string& screenId) {
   const auto focused = world_->GetUI().GetFocusedElementId(screenId);

   // 初回選択と画面の切り替えでは決定音と重ねず、同じ画面内で選択を動かしたときだけ鳴らす。
   if (screenId == selectionScreenId_ && !selectedElementId_.empty() && !focused.empty() &&
       focused != selectedElementId_) {
      PlayUISound(kCursorSoundAsset);
   }

   selectionScreenId_ = screenId;
   selectedElementId_ = focused;
}

void PauseMenuComponent::UpdateSelectionVisuals(
   const std::string& screenId, std::initializer_list<const char*> keys, float deltaTime) {
   constexpr std::array kSides{"Top", "Right", "Bottom", "Left"};
   const float offset = std::max(deltaTime, 0.0f) / kSelectionFrameLoopSeconds;
   const auto focused = world_->GetUI().GetFocusedElementId(screenId);

   // 枠だけを選択に合わせて表示し、操作部品本体と文字の表示・ヒット判定は残す。
   for (const auto* key : keys) {
      const auto* widgetObject = world_->GetUI().FindElement(screenId, key);
      const bool selected = widgetObject && widgetObject->GetEntityId() == focused;
      auto* frame = world_->GetUI().FindElement(screenId, std::string(key) + "Frame");

      if (auto* group = frame ? frame->GetComponent<GameEngine::UIGroupComponent>() : nullptr) {
         auto settings = group->DescribeSettings();

         if (settings.visible != selected) {
            settings.visible = selected;
            group->Configure(settings);
         }
      }

      // 前面スプライトが親の描画を覆うため、通常・選択・押下時の本体色を前面にも反映する。
      const auto* widget = widgetObject ? GameEngine::GetUIWidget(*widgetObject) : nullptr;
      auto* face = world_->GetUI().FindElement(screenId, std::string(key) + "FrameFace");
      auto* faceComponent = face ? face->GetComponent<GameEngine::MaterialComponent>() : nullptr;
      auto* faceMaterial = faceComponent ? faceComponent->EditMaterial() : nullptr;

      if (widget && faceMaterial) {
         const auto color = widget->GetVisualColor();
         faceMaterial->SetColor({color[0], color[1], color[2], color[3]});
      }

      if (!selected) {
         continue;
      }

      // 選択を外した帯のUVは保持し、再選択時に同じ位置から流す。
      for (const auto* side : kSides) {
         auto* object = world_->GetUI().FindElement(screenId, std::string(key) + "Frame" + side);
         auto* component = object ? object->GetComponent<GameEngine::MaterialComponent>() : nullptr;
         auto* material = component ? component->EditMaterial() : nullptr;

         if (!material) {
            continue;
         }

         auto uv = material->GetUVTransform();
         uv.m[3][0] -= offset;
         uv.m[3][0] -= std::floor(uv.m[3][0]);
         material->SetUVTransform(uv);
      }
   }
}

void PauseMenuComponent::PlayUISound(const char* clipAssetId) {
   if (auto* audio = GameEngine::EngineContext::GetAudio()) {
      audio->PlayOneShot(GameEngine::EngineContext::GetSoundClip(clipAssetId), GameEngine::AudioBus::Ui);
   }
}

void PauseMenuComponent::OnDisable() {
   if (!world_) return;

   SavePendingAudioSettings();

   if (presentationApplied_) {
      GameEngine::EngineContext::SetOverlayBackgroundBlur(0.0f);

      if (!settingsOnly_) {
         if (auto* audio = GameEngine::EngineContext::GetAudio()) {
            audio->SetBgmDucking(1.0f);
         }
      }

      presentationApplied_ = false;
   }

   // 入力配送中でも削除とコールバック解除はUISystemの安全な境界で確定する。
   auto& ui = world_->GetUI();
   ui.RemoveScreen(settingsScreenId_);
   ui.RemoveScreen(pauseScreenId_);
   settingsScreenId_.clear();
   pauseScreenId_.clear();
   selectionScreenId_.clear();
   selectedElementId_.clear();
}

void PauseMenuComponent::OnDetach() {
   world_ = nullptr;
}

nlohmann::json PauseMenuComponent::Serialize() const {
   return {{"titleScene", titleScene_}, {"settingsOnly", settingsOnly_}};
}

void PauseMenuComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) return;

   if (data.contains("titleScene") && data.at("titleScene").is_string()) {
      titleScene_ = data.at("titleScene").get<std::string>();
   }

   if (data.contains("settingsOnly") && data.at("settingsOnly").is_boolean()) {
      settingsOnly_ = data.at("settingsOnly").get<bool>();
   }
}

#ifdef USE_IMGUI
void PauseMenuComponent::DrawInspector() {
   const auto header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!ImGui::CollapsingHeader(header.c_str())) return;

   ImGui::Checkbox("Settings Only", &settingsOnly_);

   if (!settingsOnly_) {
      GameEngine::EditorUI::SceneReference("Title Scene", titleScene_);
   }
}
#endif
}
