#pragma once
#include "SoundClip.h"
#include <xaudio2.h>
#include <wrl.h>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace GameEngine {
class BgmPlayer;

/// @brief Playback category; effective gain is Master * category * voice * fade.
enum class AudioBus { Bgm, Se, Ui };

/// @brief Generation-checked playback identity. Invalid after completion or Release.
struct AudioHandle {
   uint32_t index = UINT32_MAX;
   uint32_t generation = 0;
   /// @brief Whether this has a syntactically valid index.
   explicit operator bool() const { return index != UINT32_MAX; }
};

/// @brief Observable playback state for an owned handle.
enum class AudioPlaybackState { Stopped, Playing, Paused };

/// @brief Owner of the audio device, active voices, and scene BGM player.
class Audio {
public:
   /// @brief Construct a silent audio service until Initialize is called.
   Audio();
   /// @brief Release device resources if the owner did not call Finalize.
   ~Audio();
   /// @brief Initialize Media Foundation and XAudio2; failures leave silent mode.
   void Initialize();
   /// @brief Release all voices before device and Media Foundation shutdown. Idempotent.
   void Finalize();
   /// @brief Return the device only for the legacy Sound wrapper.
   IXAudio2* GetXAudio2() const { return xAudio2_.Get(); }
   /// @brief Whether a playback device was created.
   bool IsAvailable() const { return masteringVoice_ != nullptr || testMode_; }

   /// @brief Reserve and start a voice. Returns invalid when silent, paused, or full.
   AudioHandle Create(std::shared_ptr<const SoundClip> clip, AudioBus bus, float volume = 1.0f,
      float pitch = 1.0f, bool loop = false, bool preview = false);
   /// @brief Start a stopped handle at the beginning; playing and paused handles are unchanged.
   bool Play(AudioHandle handle);
   /// @brief Restart a handle at the beginning, including a playing one.
   bool Restart(AudioHandle handle);
   /// @brief Pause a handle at its current position.
   bool Pause(AudioHandle handle);
   /// @brief Resume an explicitly paused handle at its current position.
   bool Resume(AudioHandle handle);
   /// @brief Stop and flush a handle. A later Play starts at the beginning.
   bool Stop(AudioHandle handle);
   /// @brief Release a handle and its PCM reference; stale copies can never address a new voice.
   void Release(AudioHandle handle);
   /// @brief Create an independent non-looping voice.
   AudioHandle PlayOneShot(std::shared_ptr<const SoundClip> clip, AudioBus bus,
      float volume = 1.0f, float pitch = 1.0f, bool preview = false);
   /// @brief Set a voice's individual gain.
   bool SetVolume(AudioHandle handle, float volume);
   /// @brief Set a voice's pitch ratio.
   bool SetPitch(AudioHandle handle, float pitch);
   /// @brief Move a voice to another category and update its gain immediately.
   bool SetBus(AudioHandle handle, AudioBus bus);
   /// @brief Set a voice's fade factor, from zero to one.
   bool SetFade(AudioHandle handle, float fade);
   /// @brief Set Master, BGM, SE, or UI gain; existing voices update immediately.
   void SetBusVolume(AudioBus bus, float volume);
   /// @brief Set Master gain; existing voices update immediately.
   void SetMasterVolume(float volume);
   /// @brief Pause or unpause game audio without resuming individually paused voices.
   void SetGamePaused(bool paused);
   /// @brief Stop and release every game voice, preserving only editor previews.
   void StopGameAudio();
   /// @brief Stop and release every editor preview voice.
   void StopPreviews();
   /// @brief Reclaim completed voices and advance BGM fades using real seconds.
   void Update(float realSeconds);
   /// @brief Whether a handle still owns a playback entry.
   bool IsValid(AudioHandle handle) const;
   /// @brief Return a handle's state, or nullopt after completion or release.
   std::optional<AudioPlaybackState> GetState(AudioHandle handle) const;
   /// @brief Number of currently allocated source voices.
   size_t GetVoiceCount() const;
   /// @brief The BGM controller owned by this audio service.
   BgmPlayer& GetBgmPlayer();
#ifdef MYPROJECT_NON_RELEASE
   /// @brief Start a device-free backend for deterministic audio state tests.
   void InitializeForTesting();
   /// @brief Simulate natural completion of a non-looping test voice.
   void CompleteForTesting(AudioHandle handle);
   /// @brief Inspect effective gain in device-free tests.
   float GetEffectiveVolumeForTesting(AudioHandle handle) const;
   /// @brief Count starts to distinguish Play from Restart in tests.
   uint32_t GetStartCountForTesting(AudioHandle handle) const;
#endif

private:
   using State = AudioPlaybackState;
   struct Entry {
      uint32_t generation = 1;
      std::shared_ptr<const SoundClip> clip;
      IXAudio2SourceVoice* voice = nullptr;
      XAUDIO2_BUFFER buffer{};
      AudioBus bus = AudioBus::Se;
      State state = State::Stopped;
      float volume = 1.0f, pitch = 1.0f, fade = 1.0f;
      bool loop = false, preview = false, pausedByGame = false;
      bool testVoice = false;
      uint32_t startCount = 0;
   };
   static constexpr size_t kMaxVoices = 64;
   static constexpr size_t kMaxHandles = 256;
   Entry* Find(AudioHandle handle);
   const Entry* Find(AudioHandle handle) const;
   bool Start(Entry& entry);
   void DestroyVoice(Entry& entry);
   void ApplyVolume(Entry& entry);
   void Retire(Entry& entry);

   Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
   IXAudio2MasteringVoice* masteringVoice_ = nullptr;
   bool mfStarted_ = false;
   bool gamePaused_ = false;
   bool testMode_ = false;
   float masterVolume_ = 1.0f;
   std::array<float, 3> busVolumes_{ 1.0f, 1.0f, 1.0f };
   std::array<Entry, kMaxHandles> entries_{};
   std::unique_ptr<BgmPlayer> bgmPlayer_;
};
}
