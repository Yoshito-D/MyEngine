#include "GameEngine/pch.h"
#include "GameEngine/Audio/SoundClip.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfobjects.h>
#include <limits>
#include <stdexcept>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace GameEngine {
#ifdef MYPROJECT_NON_RELEASE
std::shared_ptr<const SoundClip> SoundClip::CreateForTesting(const WAVEFORMATEX& format, std::vector<BYTE> pcm) {
   if (format.nBlockAlign == 0 || pcm.empty() || pcm.size() % format.nBlockAlign != 0) return nullptr;
   auto clip = std::shared_ptr<SoundClip>(new SoundClip());
   const BYTE* bytes = reinterpret_cast<const BYTE*>(&format);
   clip->formatWords_.resize((sizeof(format) + sizeof(uint64_t) - 1) / sizeof(uint64_t));
   std::memcpy(clip->formatWords_.data(), bytes, sizeof(format));
   clip->pcm_ = std::move(pcm);
   return clip;
}
#endif
std::shared_ptr<const SoundClip> SoundClip::Decode(const std::wstring& path) {
   ComPtr<IMFSourceReader> reader;
   if (FAILED(MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader)))
      throw std::runtime_error("Failed to open audio file");

   ComPtr<IMFMediaType> requested;
   if (FAILED(MFCreateMediaType(&requested)) ||
       FAILED(requested->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
       FAILED(requested->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)) ||
       FAILED(reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, requested.Get())))
      throw std::runtime_error("Failed to select PCM audio format");

   ComPtr<IMFMediaType> actual;
   if (FAILED(reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &actual)))
      throw std::runtime_error("Failed to read PCM audio format");
   WAVEFORMATEX* rawFormat = nullptr;
   UINT32 formatSize = 0;
   if (FAILED(MFCreateWaveFormatExFromMFMediaType(actual.Get(), &rawFormat, &formatSize)))
      throw std::runtime_error("Failed to convert PCM audio format");
   std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> formatOwner(rawFormat, &CoTaskMemFree);
   if (formatSize < sizeof(WAVEFORMATEX) || rawFormat->nBlockAlign == 0)
      throw std::runtime_error("Invalid PCM audio format");

   auto clip = std::shared_ptr<SoundClip>(new SoundClip());
   const BYTE* formatBytes = reinterpret_cast<const BYTE*>(rawFormat);
   clip->formatWords_.resize((formatSize + sizeof(uint64_t) - 1) / sizeof(uint64_t));
   std::memcpy(clip->formatWords_.data(), formatBytes, formatSize);
   for (;;) {
      DWORD stream = 0, flags = 0;
      LONGLONG timestamp = 0;
      ComPtr<IMFSample> sample;
      if (FAILED(reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, &stream, &flags, &timestamp, &sample)))
         throw std::runtime_error("Failed to decode audio sample");
      if (flags & MF_SOURCE_READERF_ERROR) throw std::runtime_error("Audio stream error");
      if (!sample) {
         if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
         continue;
      }
      ComPtr<IMFMediaBuffer> buffer;
      if (FAILED(sample->ConvertToContiguousBuffer(&buffer)))
         throw std::runtime_error("Failed to combine audio buffers");
      BYTE* bytes = nullptr;
      DWORD capacity = 0, length = 0;
      if (FAILED(buffer->Lock(&bytes, &capacity, &length)))
         throw std::runtime_error("Failed to lock audio buffer");
      try {
         if (length > std::numeric_limits<UINT32>::max() - clip->pcm_.size())
            throw std::runtime_error("Decoded audio exceeds XAudio2 buffer limit");
         if (length) clip->pcm_.insert(clip->pcm_.end(), bytes, bytes + length);
      } catch (...) {
         buffer->Unlock();
         throw;
      }
      if (FAILED(buffer->Unlock())) throw std::runtime_error("Failed to unlock audio buffer");
      if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
   }
   if (clip->pcm_.empty() || clip->pcm_.size() % rawFormat->nBlockAlign != 0)
      throw std::runtime_error("Empty or incomplete PCM audio");
   return clip;
}
}
