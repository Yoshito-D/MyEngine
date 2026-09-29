#include "pch.h"
#include "Sound.h"
#include "SoundClip.h"
#include <stdexcept>

namespace { IXAudio2* sXAudio2 = nullptr; }

namespace GameEngine {
void Sound::Initialize(IXAudio2* xAudio2) { sXAudio2 = xAudio2; }

Sound::~Sound() {
   if (sourceVoice_) {
      sourceVoice_->Stop();
      sourceVoice_->FlushSourceBuffers();
      sourceVoice_->DestroyVoice();
   }
}

void Sound::Load(const std::wstring& filepath) {
   if (!sXAudio2) return;
   LoadClip(SoundClip::Decode(filepath));
}

void Sound::LoadClip(std::shared_ptr<const SoundClip> nextClip) {
   if (!sXAudio2) return;
   if (!nextClip || nextClip->GetPcm().empty()) throw std::runtime_error("Invalid decoded audio clip");
   IXAudio2SourceVoice* nextVoice = nullptr;
   if (FAILED(sXAudio2->CreateSourceVoice(&nextVoice, nextClip->GetFormat())))
      throw std::runtime_error("Failed to create source voice");
   // Keep the old voice and its PCM intact until decode and voice creation succeed.
   if (sourceVoice_) {
      sourceVoice_->Stop();
      sourceVoice_->FlushSourceBuffers();
      sourceVoice_->DestroyVoice();
   }
   sourceVoice_ = nextVoice;
   clip_ = std::move(nextClip);
   buffer_ = {};
   isPlaying_ = false;
   isPaused_ = false;
   isLooping_ = false;
}

void Sound::Play(float volume, bool loop, bool restart) {
   if (!sourceVoice_ || !clip_) return;
   XAUDIO2_VOICE_STATE state{};
   sourceVoice_->GetState(&state);
   if (!state.BuffersQueued) isPlaying_ = false;
   if (!restart && isPlaying_) {
      sourceVoice_->SetVolume(volume);
      isLooping_ = loop;
      return;
   }
   if (!restart && state.BuffersQueued) {
      sourceVoice_->SetVolume(volume);
      sourceVoice_->Start();
      isPlaying_ = true;
      isPaused_ = false;
      return;
   }
   sourceVoice_->Stop();
   sourceVoice_->FlushSourceBuffers();
   buffer_ = {};
   buffer_.AudioBytes = static_cast<UINT32>(clip_->GetPcm().size());
   buffer_.pAudioData = clip_->GetPcm().data();
   buffer_.Flags = XAUDIO2_END_OF_STREAM;
   buffer_.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;
   if (FAILED(sourceVoice_->SubmitSourceBuffer(&buffer_))) return;
   sourceVoice_->SetVolume(volume);
   if (FAILED(sourceVoice_->Start())) return;
   isLooping_ = loop;
   isPlaying_ = true;
   isPaused_ = false;
}

void Sound::Stop() {
   // Legacy Stop is a pause: retain the queued buffer and playback position.
   if (sourceVoice_) sourceVoice_->Stop();
   isPlaying_ = false;
   isPaused_ = true;
}

void Sound::Reset() {
   if (sourceVoice_) {
      sourceVoice_->Stop();
      sourceVoice_->FlushSourceBuffers();
   }
   isPlaying_ = false;
   isPaused_ = false;
}

void Sound::SetVolume(float volume) {
   if (sourceVoice_) sourceVoice_->SetVolume(volume);
}
}
