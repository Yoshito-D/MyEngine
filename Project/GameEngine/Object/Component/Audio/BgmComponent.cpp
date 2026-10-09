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
   [](GameEngine::Object& object) -> GameEngine::IObjectComponent* { return object.AddComponent<GameEngine::BgmComponent>(); },
   GameEngine::BgmComponent::kDisplayName,
   GameEngine::ObjectType::Generic | GameEngine::ObjectType::Model |
   GameEngine::ObjectType::Sprite | GameEngine::ObjectType::UIText);
template <typename T> T Read(const nlohmann::json& data, const char* key, T fallback) {
   if (!data.contains(key)) return fallback;
   try { return data.at(key).get<T>(); } catch (...) { return fallback; }
}
}

namespace GameEngine {
BgmComponent::~BgmComponent() { Stop(); }
void BgmComponent::Prepare() {
   if (preparedAssetId_ != clipAssetId) { clip_.reset(); preparedAssetId_ = clipAssetId; }
   if (!clip_ && !clipAssetId.empty()) clip_ = EngineContext::GetSoundClip(clipAssetId);
}
bool BgmComponent::BeginRuntime() {
   if (runtimeStarted_ || !IsEnabled()) return false;
   runtimeStarted_ = true;
   Prepare();
   return playOnStart && Play();
}
bool BgmComponent::Submit(bool restart) {
   auto* audio = EngineContext::GetAudio();
   if (!IsEnabled() || !audio || !audio->IsAvailable()) return false;
   Prepare();
   if (!clip_) return false;
   auto& player = audio->GetBgmPlayer();
   if (!ownerToken_) ownerToken_ = player.NewOwnerToken();
   player.Request(ownerToken_, BgmRequest{ clipAssetId, clip_, volume, loop,
      fadeInSeconds, fadeOutSeconds, continueAcrossScenes }, restart);
   return true;
}
bool BgmComponent::Play() { return Submit(false); }
bool BgmComponent::Restart() { return Submit(true); }
void BgmComponent::Stop() {
   if (ownerToken_) {
      if (auto* audio = EngineContext::GetAudio(); audio && audio->IsAvailable())
         audio->GetBgmPlayer().ReleaseOwner(ownerToken_);
   }
   ownerToken_ = 0;
   runtimeStarted_ = false;
}
nlohmann::json BgmComponent::Serialize() const {
   return { { "clipAssetId", clipAssetId }, { "volume", volume }, { "loop", loop },
      { "playOnStart", playOnStart }, { "fadeInSeconds", fadeInSeconds },
      { "fadeOutSeconds", fadeOutSeconds }, { "continueAcrossScenes", continueAcrossScenes } };
}
void BgmComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) return;
   Stop();
   clipAssetId = Read<std::string>(data, "clipAssetId", "");
   volume = Read<float>(data, "volume", 1.0f);
   loop = Read<bool>(data, "loop", true);
   playOnStart = Read<bool>(data, "playOnStart", true);
   fadeInSeconds = Read<float>(data, "fadeInSeconds", 0.0f);
   fadeOutSeconds = Read<float>(data, "fadeOutSeconds", 0.0f);
   continueAcrossScenes = Read<bool>(data, "continueAcrossScenes", false);
   if (!std::isfinite(volume)) volume = 1.0f;
   volume = std::clamp(volume, 0.0f, 1.0f);
   if (!std::isfinite(fadeInSeconds) || fadeInSeconds < 0.0f) fadeInSeconds = 0.0f;
   if (!std::isfinite(fadeOutSeconds) || fadeOutSeconds < 0.0f) fadeOutSeconds = 0.0f;
   clip_.reset();
   Prepare();
   Configure(DescribeSettings());
}
#ifdef MYPROJECT_NON_RELEASE
void BgmComponent::SetClipForTesting(const std::string& assetId, std::shared_ptr<const SoundClip> clip) {
   clipAssetId = assetId;
   preparedAssetId_ = assetId;
   clip_ = std::move(clip);
}
#endif
#ifdef USE_IMGUI
void BgmComponent::DrawInspector() {
   auto Tr = [](const char* ja, const char* en) { return ImGuiHelper::Localize({ ja, en }); };
   if (!ImGui::CollapsingHeader(MakeObjectComponentHeaderLabel(GetTypeName()).c_str())) return;
   std::string nextId = clipAssetId;
   if (EditorUI::AssetReference(Tr("音声", "Clip"), nextId, EditorAssetType::Audio)) {
      auto nextClip = nextId.empty() ? std::shared_ptr<const SoundClip>() : EngineContext::GetSoundClip(nextId);
      if (nextId.empty() || nextClip) {
         clipAssetId = nextId;
         preparedAssetId_ = nextId;
         clip_ = std::move(nextClip);
      } else {
         Logger::Warning("[BgmComponent] Audio asset could not be loaded: " + nextId);
      }
   }
   if (!clipAssetId.empty() && !clip_) ImGui::TextUnformatted(Tr("音声が見つかりません", "Unresolved audio asset"));
   if (ImGui::SliderFloat(Tr("音量", "Volume"), &volume, 0.0f, 1.0f) && ownerToken_) Play();
   ImGui::Checkbox(Tr("ループ", "Loop"), &loop);
   ImGui::Checkbox(Tr("開始時に再生", "Play On Start"), &playOnStart);
   ImGui::DragFloat(Tr("フェードイン秒", "Fade In Seconds"), &fadeInSeconds, 0.05f, 0.0f, 60.0f);
   ImGui::DragFloat(Tr("フェードアウト秒", "Fade Out Seconds"), &fadeOutSeconds, 0.05f, 0.0f, 60.0f);
   ImGui::Checkbox(Tr("シーン間で継続", "Continue Across Scenes"), &continueAcrossScenes);
   if (ImGui::Button(Tr("試聴", "Preview")) && clip_)
      if (auto* audio = EngineContext::GetAudio()) audio->PlayOneShot(clip_, AudioBus::Bgm, volume, 1.0f, true);
   ImGui::SameLine();
   if (ImGui::Button(Tr("試聴停止", "Stop Preview")))
      if (auto* audio = EngineContext::GetAudio()) audio->StopPreviews();
   Configure(DescribeSettings());
}
#endif
}
