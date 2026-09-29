#pragma once
#include "IObjectComponent.h"
#include "Audio/SoundClip.h"
#include <memory>
#include <string>

namespace GameEngine {
/// @brief One scene BGM request; playback is owned by Audio's BgmPlayer.
class BgmComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "BgmComponent";
   static constexpr ComponentDisplayName kDisplayName{ "BGM", "BGM" };
   /// @brief Release this component's BGM request.
   ~BgmComponent() override;
   /// @brief Stable serialized type name.
   const char* GetTypeName() const override { return kTypeName; }
   /// @brief Prepare a configured clip without playback.
   void Prepare();
   /// @brief Submit playOnStart once at actual runtime start.
   bool BeginRuntime();
   /// @brief Whether this component has a playable configured startup track.
   bool HasStartupTrack() const { return IsEnabled() && playOnStart && clip_ != nullptr; }
   /// @brief Explicitly request this configured BGM without restarting the same track.
   bool Play();
   /// @brief Explicitly restart the configured BGM from the beginning.
   bool Restart();
   /// @brief Release only this component's current BGM ownership request.
   void Stop();
   /// @brief Stop the owned BGM when disabled.
   void OnDisable() override { Stop(); }
   /// @brief Remove ownership without affecting a later scene's request.
   void OnDetach() override { Stop(); }
   /// @brief Resolve the clip only; editing never starts music.
   void OnReferencesChanged(SceneWorld&) override { Prepare(); }
   /// @brief Resolve the clip only; a later runtime event starts music.
   void OnSceneLoaded(SceneWorld&) override { Prepare(); }
   /// @brief Save settings only, never voice or fade state.
   nlohmann::json Serialize() const override;
   /// @brief Restore checked settings and prepare the clip without playback.
   void Deserialize(const nlohmann::json& data) override;
#ifdef MYPROJECT_NON_RELEASE
   /// @brief Inject a decoded fixture with its asset ID for device-free tests.
   void SetClipForTesting(const std::string& assetId, std::shared_ptr<const SoundClip> clip);
#endif
#ifdef USE_IMGUI
   /// @brief Edit BGM settings and invoke independent editor preview controls.
   void DrawInspector() override;
#endif
   std::string clipAssetId;
   float volume = 1.0f;
   bool loop = true;
   bool playOnStart = true;
   float fadeInSeconds = 0.0f;
   float fadeOutSeconds = 0.0f;
   bool continueAcrossScenes = false;
private:
   bool Submit(bool restart);
   std::shared_ptr<const SoundClip> clip_;
   std::string preparedAssetId_;
   uint64_t ownerToken_ = 0;
   bool runtimeStarted_ = false;
};
}
