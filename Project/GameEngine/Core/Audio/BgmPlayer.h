#pragma once
#include "Audio.h"
#include <string>

namespace GameEngine {
/// @brief Scene-owned BGM settings used only at runtime.
struct BgmRequest {
   std::string clipAssetId;
   std::shared_ptr<const SoundClip> clip;
   float volume = 1.0f;
   bool loop = true;
   float fadeInSeconds = 0.0f;
   float fadeOutSeconds = 0.0f;
   bool continueAcrossScenes = false;
};

/// @brief At most two BGM voices, with owner tokens independent of scene addresses.
class BgmPlayer {
public:
   /// @brief Bind to its owning audio service.
   explicit BgmPlayer(Audio& audio) : audio_(audio) {}
   /// @brief Issue a unique ownership token for one component activation.
   uint64_t NewOwnerToken() { return ++nextOwner_; }
   /// @brief Request a track; the same asset continues unless restart is explicit.
   void Request(uint64_t owner, const BgmRequest& request, bool restart = false);
   /// @brief Fade out only if this is still the active request owner.
   void ReleaseOwner(uint64_t owner);
   /// @brief Keep a departing scene's track until the next scene has been examined.
   void BeginSceneChange() { sceneChanging_ = true; }
   /// @brief Apply the old track's continuation policy when no new BGM was requested.
   void EndSceneChange(bool hasNewRequest);
   /// @brief Advance fades in real seconds; caller freezes this during game pause.
   void Update(float seconds);
   /// @brief Stop both tracks and invalidate all owner tokens.
   void StopImmediate();
   /// @brief Current track ID for diagnostics and tests.
   const std::string& GetCurrentAssetId() const { return current_.assetId; }
#ifdef MYPROJECT_NON_RELEASE
   /// @brief Expose track identity to device-free tests only.
   AudioHandle GetCurrentHandleForTesting() const { return current_.handle; }
#endif

private:
   struct Track {
      AudioHandle handle;
      std::string assetId;
      uint64_t owner = 0;
      float gain = 1.0f;
      float start = 1.0f, target = 1.0f, duration = 0.0f, elapsed = 0.0f;
      float fadeOutSeconds = 0.0f;
      bool continueAcrossScenes = false;
   };
   void Fade(Track& track, float target, float seconds);
   void Release(Track& track);
   Audio& audio_;
   Track current_, outgoing_;
   uint64_t nextOwner_ = 0;
   bool sceneChanging_ = false;
};
}
