#include "GameEngine/pch.h"
#include "GameEngine/Object/Component/Audio/AudioSourceComponent.h"
#include "GameEngine/Object/Component/Base/ComponentRegistry.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Object/Object.h"
#include <cmath>
#include <limits>

#ifdef USE_IMGUI
#include "GameEngine/Editor/AudioAssetWidget.h"
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#include <imgui.h>
#include <cstring>
#endif

namespace {
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::AudioSourceComponent::kTypeName,
   [](GameEngine::Object& object) -> GameEngine::IObjectComponent* { return object.AddComponent<GameEngine::AudioSourceComponent>(); },
   GameEngine::AudioSourceComponent::kDisplayName,
   GameEngine::ObjectType::Generic | GameEngine::ObjectType::Model |
   GameEngine::ObjectType::Sprite | GameEngine::ObjectType::UIText);

template <typename T> T Read(const nlohmann::json& data, const char* key, T fallback) {
   if (!data.contains(key)) return fallback;
   try { return data.at(key).get<T>(); } catch (...) { return fallback; }
}
}

namespace GameEngine {
AudioSourceComponent::~AudioSourceComponent() { StopAll(); }
size_t AudioSourceComponent::FindSlot(const std::string& name) const {
   for (size_t i = 0; i < slots_.size(); ++i) if (slots_[i].name == name) return i;
   return slots_.size();
}
bool AudioSourceComponent::AddSlot(Slot slot) {
   if (slot.name.empty() || FindSlot(slot.name) != slots_.size() ||
       !std::isfinite(slot.volume) || !std::isfinite(slot.pitch) || slot.pitch <= 0.0f) return false;
   slot.volume = std::clamp(slot.volume, 0.0f, 1.0f);
   slots_.push_back(std::move(slot));
   runtime_.emplace_back();
   runtime_.back().clip = EngineContext::GetSoundClip(slots_.back().clipAssetId);
   return true;
}
bool AudioSourceComponent::RemoveSlot(const std::string& name) {
   const size_t i = FindSlot(name);
   if (i == slots_.size()) return false;
   if (auto* audio = EngineContext::GetAudio()) audio->Release(runtime_[i].handle);
   slots_.erase(slots_.begin() + i);
   runtime_.erase(runtime_.begin() + i);
   return true;
}
bool AudioSourceComponent::SetClip(const std::string& name, const std::string& assetId) {
   const size_t i = FindSlot(name);
   if (i == slots_.size()) return false;
   auto clip = EngineContext::GetSoundClip(assetId);
   const bool resolved = clip != nullptr || assetId.empty();
   if (auto* audio = EngineContext::GetAudio()) audio->Release(runtime_[i].handle);
   slots_[i].clipAssetId = assetId;
   runtime_[i].clip = std::move(clip);
   runtime_[i].handle = {};
   return resolved;
}
void AudioSourceComponent::Prepare() {
   for (size_t i = 0; i < slots_.size(); ++i)
      if (!runtime_[i].clip && !slots_[i].clipAssetId.empty())
         runtime_[i].clip = EngineContext::GetSoundClip(slots_[i].clipAssetId);
}
void AudioSourceComponent::BeginRuntime() {
   if (runtimeStarted_ || !IsEnabled()) return;
   runtimeStarted_ = true;
   Prepare();
   for (const Slot& slot : slots_) if (slot.playOnStart) Play(slot.name);
}
bool AudioSourceComponent::Play(const std::string& name) {
   const size_t i = FindSlot(name);
   auto* audio = EngineContext::GetAudio();
   if (i == slots_.size() || !audio || !IsEnabled() || !runtime_[i].clip) return false;
   if (audio->IsValid(runtime_[i].handle)) return audio->Play(runtime_[i].handle);
   const Slot& slot = slots_[i];
   runtime_[i].handle = audio->Create(runtime_[i].clip, slot.bus, slot.volume, slot.pitch, slot.loop);
   return static_cast<bool>(runtime_[i].handle);
}
bool AudioSourceComponent::Restart(const std::string& name) {
   const size_t i = FindSlot(name);
   if (i == slots_.size()) return false;
   if (auto* audio = EngineContext::GetAudio(); audio && audio->IsValid(runtime_[i].handle))
      return audio->Restart(runtime_[i].handle);
   return Play(name);
}
bool AudioSourceComponent::Pause(const std::string& name) {
   const size_t i = FindSlot(name);
   auto* audio = EngineContext::GetAudio();
   return i != slots_.size() && audio && audio->Pause(runtime_[i].handle);
}
bool AudioSourceComponent::Resume(const std::string& name) {
   const size_t i = FindSlot(name);
   auto* audio = EngineContext::GetAudio();
   return i != slots_.size() && audio && audio->Resume(runtime_[i].handle);
}
bool AudioSourceComponent::Stop(const std::string& name) {
   const size_t i = FindSlot(name);
   auto* audio = EngineContext::GetAudio();
   return i != slots_.size() && audio && audio->Stop(runtime_[i].handle);
}
AudioHandle AudioSourceComponent::PlayOneShot(const std::string& name) {
   const size_t i = FindSlot(name);
   auto* audio = EngineContext::GetAudio();
   if (i == slots_.size() || !audio || !IsEnabled() || !runtime_[i].clip) return {};
   oneShots_.erase(std::remove_if(oneShots_.begin(), oneShots_.end(),
      [audio](AudioHandle handle) { return !audio->IsValid(handle); }), oneShots_.end());
   const Slot& slot = slots_[i];
   AudioHandle handle = audio->PlayOneShot(runtime_[i].clip, slot.bus, slot.volume, slot.pitch);
   if (handle) oneShots_.push_back(handle);
   return handle;
}
void AudioSourceComponent::StopAll() {
   if (auto* audio = EngineContext::GetAudio()) {
      for (auto& slot : runtime_) { audio->Release(slot.handle); slot.handle = {}; }
      for (auto handle : oneShots_) audio->Release(handle);
   }
   oneShots_.clear();
   runtimeStarted_ = false;
}
nlohmann::json AudioSourceComponent::Serialize() const {
   nlohmann::json values = nlohmann::json::array();
   for (const Slot& slot : slots_) values.push_back({
      { "name", slot.name }, { "clipAssetId", slot.clipAssetId },
      { "bus", slot.bus == AudioBus::Ui ? "UI" : "SE" }, { "volume", slot.volume },
      { "pitch", slot.pitch }, { "loop", slot.loop }, { "playOnStart", slot.playOnStart }
   });
   return { { "slots", std::move(values) } };
}
void AudioSourceComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object() || !data.contains("slots") || !data.at("slots").is_array()) return;
   StopAll();
   slots_.clear(); runtime_.clear();
   for (const auto& value : data.at("slots")) {
      if (!value.is_object()) continue;
      Slot slot;
      slot.name = Read<std::string>(value, "name", "");
      slot.clipAssetId = Read<std::string>(value, "clipAssetId", "");
      slot.bus = Read<std::string>(value, "bus", "SE") == "UI" ? AudioBus::Ui : AudioBus::Se;
      slot.volume = Read<float>(value, "volume", 1.0f);
      slot.pitch = Read<float>(value, "pitch", 1.0f);
      slot.loop = Read<bool>(value, "loop", false);
      slot.playOnStart = Read<bool>(value, "playOnStart", false);
      if (!AddSlot(std::move(slot))) Logger::Warning("Invalid or duplicate audio slot skipped");
   }
}
#ifdef MYPROJECT_NON_RELEASE
void AudioSourceComponent::SetClipForTesting(const std::string& name, std::shared_ptr<const SoundClip> clip) {
   const size_t i = FindSlot(name);
   if (i < slots_.size()) runtime_[i].clip = std::move(clip);
}
AudioHandle AudioSourceComponent::GetHandleForTesting(const std::string& name) const {
   const size_t i = FindSlot(name);
   return i < slots_.size() ? runtime_[i].handle : AudioHandle{};
}
#endif
#ifdef USE_IMGUI
void AudioSourceComponent::DrawInspector() {
   auto Tr = [](const char* ja, const char* en) { return ImGuiHelper::Localize({ ja, en }); };
   if (!ImGui::CollapsingHeader(MakeObjectComponentHeaderLabel(GetTypeName()).c_str())) return;
   if (ImGui::Button(Tr("スロット追加", "Add Slot"))) {
      unsigned number = 1;
      while (FindSlot("sound" + std::to_string(number)) != slots_.size()) ++number;
      AddSlot(Slot{ "sound" + std::to_string(number) });
   }
   std::string removeName;
   for (size_t i = 0; i < slots_.size(); ++i) {
      Slot& slot = slots_[i];
      ImGui::PushID(static_cast<int>(i));
      if (ImGui::TreeNode(slot.name.c_str())) {
         char name[128]{}; strncpy_s(name, slot.name.c_str(), _TRUNCATE);
         if (ImGui::InputText(Tr("名前", "Name"), name, sizeof(name)) && name[0] &&
             (name == slot.name || FindSlot(name) == slots_.size())) slot.name = name;
         std::string clipId = slot.clipAssetId;
         if (DrawAudioAssetWidget(Tr("音声", "Clip"), clipId)) SetClip(slot.name, clipId);
         if (!slot.clipAssetId.empty() && !runtime_[i].clip) ImGui::TextUnformatted(Tr("音声が見つかりません", "Unresolved audio asset"));
         int bus = slot.bus == AudioBus::Ui ? 1 : 0;
         if (ImGui::Combo(Tr("区分", "Bus"), &bus, "SE\0UI\0")) {
            slot.bus = bus ? AudioBus::Ui : AudioBus::Se;
            if (auto* audio = EngineContext::GetAudio()) audio->SetBus(runtime_[i].handle, slot.bus);
         }
         if (ImGui::SliderFloat(Tr("音量", "Volume"), &slot.volume, 0.0f, 1.0f))
            if (auto* audio = EngineContext::GetAudio()) audio->SetVolume(runtime_[i].handle, slot.volume);
         if (ImGui::SliderFloat(Tr("ピッチ", "Pitch"), &slot.pitch, 0.1f, 2.0f))
            if (auto* audio = EngineContext::GetAudio()) audio->SetPitch(runtime_[i].handle, slot.pitch);
         ImGui::Checkbox(Tr("ループ", "Loop"), &slot.loop);
         ImGui::Checkbox(Tr("開始時に再生", "Play On Start"), &slot.playOnStart);
         if (ImGui::Button(Tr("試聴", "Preview")) && runtime_[i].clip)
            if (auto* audio = EngineContext::GetAudio()) audio->PlayOneShot(runtime_[i].clip, slot.bus, slot.volume, slot.pitch, true);
         ImGui::SameLine();
         if (ImGui::Button(Tr("試聴停止", "Stop Preview")))
            if (auto* audio = EngineContext::GetAudio()) audio->StopPreviews();
         ImGui::SameLine();
         if (ImGui::Button(Tr("削除", "Remove"))) removeName = slot.name;
         ImGui::TreePop();
      }
      ImGui::PopID();
   }
   if (!removeName.empty()) RemoveSlot(removeName);
}
#endif
}
