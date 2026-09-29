#pragma once
#include "IObjectComponent.h"
#include "Audio/Audio.h"
#include <memory>
#include <string>
#include <vector>

namespace GameEngine {
/// @brief Named sound slots on any playable Object. One component holds many sounds.
class AudioSourceComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "AudioSourceComponent";
   static constexpr ComponentDisplayName kDisplayName{ "音声ソース", "Audio Source" };
   /// @brief Persisted slot settings; clip and handle are runtime-only.
   struct Slot {
      std::string name;
      std::string clipAssetId;
      AudioBus bus = AudioBus::Se;
      float volume = 1.0f;
      float pitch = 1.0f;
      bool loop = false;
      bool playOnStart = false;
   };
   /// @brief Stop owned playback when destroyed even outside container Detach.
   ~AudioSourceComponent() override;
   /// @brief Stable serialized component type.
   const char* GetTypeName() const override { return kTypeName; }
   /// @brief Add a uniquely named slot; false on a duplicate or empty name.
   bool AddSlot(Slot slot);
   /// @brief Remove one slot and stop its persistent voice.
   bool RemoveSlot(const std::string& name);
   /// @brief Read-only configured slots.
   const std::vector<Slot>& GetSlots() const { return slots_; }
   /// @brief Replace one slot's clip and prepare it now, without auto-playing.
   bool SetClip(const std::string& name, const std::string& assetId);
   /// @brief Prepare all clip references without starting playback.
   void Prepare();
   /// @brief Run playOnStart once for the current runtime activation.
   void BeginRuntime();
   /// @brief Play a named persistent slot; repeated calls do not overlap.
   bool Play(const std::string& name);
   /// @brief Restart a named persistent slot from the beginning.
   bool Restart(const std::string& name);
   /// @brief Pause only the persistent voice for a named slot.
   bool Pause(const std::string& name);
   /// @brief Resume only the persistent voice for a named slot.
   bool Resume(const std::string& name);
   /// @brief Stop only the persistent voice for a named slot; one-shots continue.
   bool Stop(const std::string& name);
   /// @brief Fire an independent, non-looping voice, regardless of slot.loop.
   AudioHandle PlayOneShot(const std::string& name);
   /// @brief Stop persistent and one-shot voices owned by this component.
   void StopAll();
   /// @brief Stop owned voices when disabled.
   void OnDisable() override { StopAll(); }
   /// @brief Stop owned voices when detached.
   void OnDetach() override { StopAll(); }
   /// @brief Resolve clips only; never auto-play during editing.
   void OnReferencesChanged(SceneWorld&) override { Prepare(); }
   /// @brief Resolve clips only; runtime start is an explicit later event.
   void OnSceneLoaded(SceneWorld&) override { Prepare(); }
   /// @brief Save only slot settings.
   nlohmann::json Serialize() const override;
   /// @brief Restore valid slot settings and prepare clips, without playback.
   void Deserialize(const nlohmann::json& data) override;
#ifdef MYPROJECT_NON_RELEASE
   /// @brief Inject a decoded fixture into a named slot for device-free tests.
   void SetClipForTesting(const std::string& name, std::shared_ptr<const SoundClip> clip);
   /// @brief Inspect the persistent handle of a named slot in tests.
   AudioHandle GetHandleForTesting(const std::string& name) const;
#endif
#ifdef USE_IMGUI
   /// @brief Edit slots and preview audio without making the scene dirty for preview actions.
   void DrawInspector() override;
#endif
private:
   struct RuntimeSlot { std::shared_ptr<const SoundClip> clip; AudioHandle handle; };
   size_t FindSlot(const std::string& name) const;
   std::vector<Slot> slots_;
   std::vector<RuntimeSlot> runtime_;
   std::vector<AudioHandle> oneShots_;
   bool runtimeStarted_ = false;
};
}
