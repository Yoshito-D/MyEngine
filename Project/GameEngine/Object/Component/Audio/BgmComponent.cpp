#include "GameEngine/pch.h"
#include "GameEngine/Object/Component/Audio/BgmComponent.h"
#include "GameEngine/Object/Component/Base/ComponentRegistry.h"
#include "GameEngine/Audio/Audio.h"
#include "GameEngine/Audio/BgmPlayer.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Object/Object.h"
#include <cmath>

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#include <imgui.h>
#endif

namespace {
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::BgmComponent::kTypeName,
   [](GameEngine::Object& object) -> GameEngine::IObjectComponent* {
      return object.AddComponent<GameEngine::BgmComponent>();
   },
   GameEngine::BgmComponent::kDisplayName,
   GameEngine::ObjectType::Generic | GameEngine::ObjectType::Model | GameEngine::ObjectType::Sprite |
      GameEngine::ObjectType::UIText);

template <typename T> T Read(const nlohmann::json& data, const char* key, T fallback) {
   if (!data.contains(key)) {
      return fallback;
   }

   try {
      return data.at(key).get<T>();
   } catch (...) {
      return fallback;
   }
}
}

namespace GameEngine {
BgmComponent::~BgmComponent() {
   Stop();
}

void BgmComponent::Prepare() {
   if (preparedAssetId_ != settings_.clipAssetId) {
      clip_.reset();
      preparedAssetId_ = settings_.clipAssetId;
   }

   if (!clip_ && !settings_.clipAssetId.empty()) {
      clip_ = EngineContext::GetSoundClip(settings_.clipAssetId);
   }
}

bool BgmComponent::BeginRuntime() {
   if (runtimeStarted_ || !IsEnabled()) {
      return false;
   }

   runtimeStarted_ = true;
   Prepare();
   return settings_.playOnStart && Play();
}

bool BgmComponent::Submit(bool restart) {
   auto* audio = EngineContext::GetAudio();

   if (!IsEnabled() || !audio || !audio->IsAvailable()) {
      return false;
   }

   Prepare();

   if (!clip_) {
      return false;
   }

   if (!ownerToken_) {
      ownerToken_ = audio->CreateBgmOwner();
   }

   audio->RequestBgm(ownerToken_,
      BgmRequest{settings_.clipAssetId, clip_, settings_.volume, settings_.loop, settings_.fadeInSeconds,
         settings_.fadeOutSeconds, settings_.continueAcrossScenes},
      restart);
   return true;
}

bool BgmComponent::Play() {
   return Submit(false);
}

bool BgmComponent::Restart() {
   return Submit(true);
}

void BgmComponent::Stop() {
   if (ownerToken_) {
      if (auto* audio = EngineContext::GetAudio(); audio && audio->IsAvailable()) {
         audio->ReleaseBgmOwner(ownerToken_);
      }
   }

   ownerToken_ = 0;
   runtimeStarted_ = false;
}

nlohmann::json BgmComponent::Serialize() const {
   return {{"clipAssetId", settings_.clipAssetId}, {"volume", settings_.volume}, {"loop", settings_.loop},
      {"playOnStart", settings_.playOnStart}, {"fadeInSeconds", settings_.fadeInSeconds},
      {"fadeOutSeconds", settings_.fadeOutSeconds}, {"continueAcrossScenes", settings_.continueAcrossScenes}};
}

void BgmComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) return;

   Stop();
   Settings settings;
   settings.clipAssetId = Read<std::string>(data, "clipAssetId", settings.clipAssetId);
   settings.volume = Read<float>(data, "volume", settings.volume);
   settings.loop = Read<bool>(data, "loop", settings.loop);
   settings.playOnStart = Read<bool>(data, "playOnStart", settings.playOnStart);
   settings.fadeInSeconds = Read<float>(data, "fadeInSeconds", settings.fadeInSeconds);
   settings.fadeOutSeconds = Read<float>(data, "fadeOutSeconds", settings.fadeOutSeconds);
   settings.continueAcrossScenes = Read<bool>(data, "continueAcrossScenes", settings.continueAcrossScenes);

   clip_.reset();
   Configure(settings);
}
#ifdef MYPROJECT_NON_RELEASE
void BgmComponent::SetClipForTesting(const std::string& assetId, std::shared_ptr<const SoundClip> clip) {
   settings_.clipAssetId = assetId;
   preparedAssetId_ = assetId;
   clip_ = std::move(clip);
}
#endif
#ifdef USE_IMGUI
void BgmComponent::DrawInspector() {
   auto Tr = [](const char* ja, const char* en) { return ImGuiHelper::Localize({ja, en}); };

   if (!ImGui::CollapsingHeader(MakeObjectComponentHeaderLabel(GetTypeName()).c_str())) return;

   auto settings = DescribeSettings();
   std::string nextId = settings.clipAssetId;

   if (EditorUI::AssetReference(Tr("音声", "Clip"), nextId, EditorAssetType::Audio)) {
      auto nextClip = nextId.empty() ? std::shared_ptr<const SoundClip>() : EngineContext::GetSoundClip(nextId);

      if (nextId.empty() || nextClip) {
         settings.clipAssetId = nextId;
         preparedAssetId_ = nextId;
         clip_ = std::move(nextClip);
      } else {
         Logger::Warning("[BgmComponent] Audio asset could not be loaded: " + nextId);
      }
   }

   if (!settings.clipAssetId.empty() && !clip_) {
      ImGui::TextUnformatted(Tr("音声が見つかりません", "Unresolved audio asset"));
   }

   const bool volumeChanged = ImGui::SliderFloat(Tr("音量", "Volume"), &settings.volume, 0.0f, 1.0f);

   ImGui::Checkbox(Tr("ループ", "Loop"), &settings.loop);
   ImGui::Checkbox(Tr("開始時に再生", "Play On Start"), &settings.playOnStart);
   ImGui::DragFloat(Tr("フェードイン秒", "Fade In Seconds"), &settings.fadeInSeconds, 0.05f, 0.0f, 60.0f);
   ImGui::DragFloat(Tr("フェードアウト秒", "Fade Out Seconds"), &settings.fadeOutSeconds, 0.05f, 0.0f, 60.0f);
   ImGui::Checkbox(Tr("シーン間で継続", "Continue Across Scenes"), &settings.continueAcrossScenes);

   Configure(settings);

   if (volumeChanged && ownerToken_) {
      Play();
   }

   if (ImGui::Button(Tr("試聴", "Preview")) && clip_) {
      if (auto* audio = EngineContext::GetAudio()) {
         audio->PlayOneShot(clip_, AudioBus::Bgm, settings_.volume, 1.0f, true);
      }
   }

   ImGui::SameLine();

   if (ImGui::Button(Tr("試聴停止", "Stop Preview"))) {
      if (auto* audio = EngineContext::GetAudio()) {
         audio->StopPreviews();
      }
   }
}
#endif
}

namespace GameEngine {

BgmComponent::Settings BgmComponent::DescribeSettings() const {
   return settings_;
}

void BgmComponent::Configure(const Settings& requested) {
   auto settings = requested;
   [[maybe_unused]] const Settings defaults;

   if (!std::isfinite(settings.volume)) {
      settings.volume = defaults.volume;
   }

   if (!std::isfinite(settings.fadeInSeconds)) {
      settings.fadeInSeconds = defaults.fadeInSeconds;
   }

   if (!std::isfinite(settings.fadeOutSeconds)) {
      settings.fadeOutSeconds = defaults.fadeOutSeconds;
   }

   settings.volume = std::clamp(settings.volume, 0.0f, 1.0f);
   settings.fadeInSeconds = std::max(settings.fadeInSeconds, 0.0f);
   settings.fadeOutSeconds = std::max(settings.fadeOutSeconds, 0.0f);
   settings_ = std::move(settings);
   Prepare();
}

} // namespace GameEngine
