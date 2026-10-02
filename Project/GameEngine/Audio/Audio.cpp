#include "GameEngine/pch.h"
#include "GameEngine/Audio/Audio.h"
#include "GameEngine/Audio/BgmPlayer.h"
#include "GameEngine/Audio/Sound.h"
#include <mfapi.h>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")

namespace GameEngine {
namespace {
float Gain(float value) { return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f; }
bool ValidPitch(float value) { return std::isfinite(value) && value >= XAUDIO2_MIN_FREQ_RATIO && value <= XAUDIO2_MAX_FREQ_RATIO; }
}

Audio::Audio() : bgmPlayer_(std::make_unique<BgmPlayer>(*this)) {}
Audio::~Audio() { Finalize(); }

void Audio::Initialize() {
   if (xAudio2_ || mfStarted_) return;
   if (FAILED(MFStartup(MF_VERSION))) {
      Logger::Warning("Media Foundation audio initialization failed; audio is disabled");
      return;
   }
   mfStarted_ = true;
   if (FAILED(XAudio2Create(xAudio2_.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR)) ||
       FAILED(xAudio2_->CreateMasteringVoice(&masteringVoice_))) {
      Logger::Warning("XAudio2 initialization failed; audio is disabled");
      Finalize();
      return;
   }
}

void Audio::Finalize() {
   if (bgmPlayer_) bgmPlayer_->StopImmediate();
   for (auto& entry : entries_) Retire(entry);
   Sound::Initialize(nullptr);
   if (xAudio2_) xAudio2_->StopEngine();
   if (masteringVoice_) { masteringVoice_->DestroyVoice(); masteringVoice_ = nullptr; }
   xAudio2_.Reset();
   if (mfStarted_) { MFShutdown(); mfStarted_ = false; }
   gamePaused_ = false;
   testMode_ = false;
}

Audio::Entry* Audio::Find(AudioHandle handle) {
   if (handle.index >= entries_.size()) return nullptr;
   Entry& entry = entries_[handle.index];
   // インデックスを再利用しても、世代番号が異なる古いハンドルは無効として扱う。
   return entry.generation == handle.generation && entry.clip ? &entry : nullptr;
}
const Audio::Entry* Audio::Find(AudioHandle handle) const {
   return const_cast<Audio*>(this)->Find(handle);
}
bool Audio::IsValid(AudioHandle handle) const { return Find(handle) != nullptr; }
std::optional<AudioPlaybackState> Audio::GetState(AudioHandle handle) const {
   if (const Entry* entry = Find(handle)) return entry->state;
   return std::nullopt;
}
size_t Audio::GetVoiceCount() const {
   return std::count_if(entries_.begin(), entries_.end(), [](const Entry& entry) { return entry.voice != nullptr || entry.testVoice; });
}

AudioHandle Audio::Create(std::shared_ptr<const SoundClip> clip, AudioBus bus, float volume, float pitch, bool loop, bool preview) {
   if (!IsAvailable() || !clip || clip->GetPcm().empty() || !ValidPitch(pitch) ||
       !std::isfinite(volume) || static_cast<size_t>(bus) >= busVolumes_.size() ||
       (gamePaused_ && !preview)) return {};
   // 決定的な上限を適用する前に、完了したワンショットを回収する。
   Update(0.0f);
   if (GetVoiceCount() >= kMaxVoices) return {};
   for (uint32_t index = 0; index < entries_.size(); ++index) {
      Entry& entry = entries_[index];
      if (entry.clip) continue;
      entry.clip = std::move(clip);
      entry.bus = bus;
      entry.volume = Gain(volume);
      entry.pitch = pitch;
      entry.fade = 1.0f;
      entry.loop = loop;
      entry.preview = preview;
      if (!Start(entry)) { Retire(entry); return {}; }
      return { index, entry.generation };
   }
   return {};
}

bool Audio::Start(Entry& entry) {
   if (!entry.clip || !IsAvailable() || GetVoiceCount() >= kMaxVoices) return false;
   // テスト時はXAudio2を使わず状態だけを進め、実機と同じハンドル検証を通す。
   if (testMode_) { entry.testVoice = true; entry.state = State::Playing; ++entry.startCount; return true; }
   if (!entry.voice && FAILED(xAudio2_->CreateSourceVoice(&entry.voice, entry.clip->GetFormat()))) return false;
   entry.buffer = {};
   entry.buffer.AudioBytes = static_cast<UINT32>(entry.clip->GetPcm().size());
   entry.buffer.pAudioData = entry.clip->GetPcm().data();
   entry.buffer.Flags = XAUDIO2_END_OF_STREAM;
   entry.buffer.LoopCount = entry.loop ? XAUDIO2_LOOP_INFINITE : 0;
   if (FAILED(entry.voice->SubmitSourceBuffer(&entry.buffer)) ||
       FAILED(entry.voice->SetFrequencyRatio(entry.pitch))) {
      DestroyVoice(entry);
      return false;
   }
   ApplyVolume(entry);
   if (FAILED(entry.voice->Start())) { DestroyVoice(entry); return false; }
   entry.state = State::Playing;
   return true;
}

void Audio::DestroyVoice(Entry& entry) {
   entry.testVoice = false;
   entry.state = State::Stopped;
   if (!entry.voice) return;
   entry.voice->Stop();
   entry.voice->FlushSourceBuffers();
   entry.voice->DestroyVoice();
   entry.voice = nullptr;
   entry.state = State::Stopped;
}
void Audio::Retire(Entry& entry) {
   DestroyVoice(entry);
   entry.clip.reset();
   entry.generation = entry.generation == UINT32_MAX ? 1 : entry.generation + 1;
   entry.pausedByGame = false;
   entry.startCount = 0;
}

bool Audio::Play(AudioHandle handle) {
   Entry* entry = Find(handle);
   if (!entry) return false;
   if (entry->state != State::Stopped) return true;
   if (gamePaused_ && !entry->preview) return false;
   return Start(*entry);
}
bool Audio::Restart(AudioHandle handle) {
   Entry* entry = Find(handle);
   if (!entry || (gamePaused_ && !entry->preview)) return false;
   DestroyVoice(*entry);
   return Start(*entry);
}
bool Audio::Pause(AudioHandle handle) {
   Entry* entry = Find(handle);
   if (!entry) return false;
   if (entry->state == State::Playing && (entry->voice || entry->testVoice)) {
      if (entry->voice) entry->voice->Stop();
      entry->state = State::Paused;
   }
   entry->pausedByGame = false;
   return true;
}
bool Audio::Resume(AudioHandle handle) {
   Entry* entry = Find(handle);
   if (!entry || (gamePaused_ && !entry->preview)) return false;
   if (entry->state == State::Paused && (entry->voice || entry->testVoice)) {
      if (entry->voice && FAILED(entry->voice->Start())) return false;
      entry->state = State::Playing;
   }
   entry->pausedByGame = false;
   return true;
}
bool Audio::Stop(AudioHandle handle) {
   Entry* entry = Find(handle);
   if (!entry) return false;
   DestroyVoice(*entry);
   entry->pausedByGame = false;
   return true;
}
void Audio::Release(AudioHandle handle) { if (Entry* entry = Find(handle)) Retire(*entry); }
AudioHandle Audio::PlayOneShot(std::shared_ptr<const SoundClip> clip, AudioBus bus, float volume, float pitch, bool preview) {
   return Create(std::move(clip), bus, volume, pitch, false, preview);
}

void Audio::ApplyVolume(Entry& entry) {
   if (entry.voice) entry.voice->SetVolume(masterVolume_ * busVolumes_[static_cast<size_t>(entry.bus)] * entry.volume * entry.fade);
}
bool Audio::SetVolume(AudioHandle handle, float volume) {
   Entry* entry = Find(handle);
   if (!entry || !std::isfinite(volume)) return false;
   entry->volume = Gain(volume);
   ApplyVolume(*entry);
   return true;
}
bool Audio::SetPitch(AudioHandle handle, float pitch) {
   Entry* entry = Find(handle);
   if (!entry || !ValidPitch(pitch)) return false;
   if (entry->voice && FAILED(entry->voice->SetFrequencyRatio(pitch))) return false;
   entry->pitch = pitch;
   return true;
}
bool Audio::SetBus(AudioHandle handle, AudioBus bus) {
   Entry* entry = Find(handle);
   if (!entry || static_cast<size_t>(bus) >= busVolumes_.size()) return false;
   entry->bus = bus;
   ApplyVolume(*entry);
   return true;
}
bool Audio::SetFade(AudioHandle handle, float fade) {
   Entry* entry = Find(handle);
   if (!entry || !std::isfinite(fade)) return false;
   entry->fade = Gain(fade);
   ApplyVolume(*entry);
   return true;
}
void Audio::SetBusVolume(AudioBus bus, float volume) {
   if (!std::isfinite(volume) || static_cast<size_t>(bus) >= busVolumes_.size()) return;
   busVolumes_[static_cast<size_t>(bus)] = Gain(volume);
   for (auto& entry : entries_) ApplyVolume(entry);
}
void Audio::SetMasterVolume(float volume) {
   if (!std::isfinite(volume)) return;
   masterVolume_ = Gain(volume);
   for (auto& entry : entries_) ApplyVolume(entry);
}
void Audio::SetGamePaused(bool paused) {
   if (gamePaused_ == paused) return;
   gamePaused_ = paused;
   // ゲーム一時停止で止めたボイスだけを再開し、ユーザーが個別に止めたボイスは維持する。
   for (auto& entry : entries_) {
      if (!entry.clip || entry.preview || (!entry.voice && !entry.testVoice)) continue;
      if (paused && entry.state == State::Playing) {
         if (entry.voice) entry.voice->Stop();
         entry.state = State::Paused;
         entry.pausedByGame = true;
      } else if (!paused && entry.pausedByGame) {
         entry.pausedByGame = false;
         if (!entry.voice || SUCCEEDED(entry.voice->Start())) entry.state = State::Playing;
      }
   }
}
void Audio::StopGameAudio() {
   if (bgmPlayer_) bgmPlayer_->StopImmediate();
   for (auto& entry : entries_) if (entry.clip && !entry.preview) Retire(entry);
   gamePaused_ = false;
}
void Audio::StopPreviews() {
   for (auto& entry : entries_) if (entry.clip && entry.preview) Retire(entry);
}
void Audio::Update(float realSeconds) {
   // XAudio2が自然終了を通知する前にキューを確認し、完了したワンショットを再利用可能にする。
   for (auto& entry : entries_) {
      if (!entry.clip || !entry.voice || entry.state != State::Playing || entry.loop) continue;
      XAUDIO2_VOICE_STATE state{};
      entry.voice->GetState(&state);
      if (!state.BuffersQueued) Retire(entry);
   }
   if (bgmPlayer_ && !gamePaused_ && std::isfinite(realSeconds) && realSeconds > 0.0f)
      bgmPlayer_->Update(realSeconds);
}
BgmPlayer& Audio::GetBgmPlayer() { return *bgmPlayer_; }
#ifdef MYPROJECT_NON_RELEASE
void Audio::InitializeForTesting() {
   Finalize();
   testMode_ = true;
}
void Audio::CompleteForTesting(AudioHandle handle) {
   if (Entry* entry = Find(handle); entry && entry->testVoice && !entry->loop) Retire(*entry);
}
float Audio::GetEffectiveVolumeForTesting(AudioHandle handle) const {
   const Entry* entry = Find(handle);
   return entry ? masterVolume_ * busVolumes_[static_cast<size_t>(entry->bus)] * entry->volume * entry->fade : 0.0f;
}
uint32_t Audio::GetStartCountForTesting(AudioHandle handle) const {
   const Entry* entry = Find(handle);
   return entry ? entry->startCount : 0;
}
#endif
}
