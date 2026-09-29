#pragma once

#include <xaudio2.h>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace GameEngine {
/// @brief One decoded PCM asset shared by every playback voice.
class SoundClip final {
public:
   /// @brief Decode WAV or MP3 through Media Foundation; throws on failure.
   /// @param path Native filesystem path, including non-ASCII characters.
   /// @return An immutable clip with a complete WAVEFORMATEX block.
   static std::shared_ptr<const SoundClip> Decode(const std::wstring& path);
#ifdef MYPROJECT_NON_RELEASE
   /// @brief Construct a small validated PCM fixture without Media Foundation for tests.
   static std::shared_ptr<const SoundClip> CreateForTesting(const WAVEFORMATEX& format, std::vector<BYTE> pcm);
#endif

   /// @brief Format block used to create XAudio2 source voices.
   const WAVEFORMATEX* GetFormat() const { return reinterpret_cast<const WAVEFORMATEX*>(formatWords_.data()); }
   /// @brief PCM data kept alive while any voice references it.
   const std::vector<BYTE>& GetPcm() const { return pcm_; }

private:
   std::vector<uint64_t> formatWords_;
   std::vector<BYTE> pcm_;
};
}
