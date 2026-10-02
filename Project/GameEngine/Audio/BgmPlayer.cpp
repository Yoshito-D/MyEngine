#include "GameEngine/pch.h"
#include "GameEngine/Audio/BgmPlayer.h"
#include <algorithm>
#include <cmath>

namespace GameEngine {
void BgmPlayer::Release(Track& track) {
   audio_.Release(track.handle);
   track = {};
}
void BgmPlayer::Fade(Track& track, float target, float seconds) {
   if (!audio_.IsValid(track.handle)) return;
   track.start = track.gain;
   track.target = target;
   track.duration = std::isfinite(seconds) ? std::max(0.0f, seconds) : 0.0f;
   track.elapsed = 0.0f;
   if (track.duration == 0.0f) {
      track.gain = target;
      audio_.SetFade(track.handle, target);
      if (target == 0.0f) Release(track);
   }
}
void BgmPlayer::Request(uint64_t owner, const BgmRequest& request, bool restart) {
   if (!owner || !request.clip || request.clipAssetId.empty()) return;
   if (current_.assetId == request.clipAssetId && audio_.IsValid(current_.handle)) {
      current_.owner = owner;
      current_.continueAcrossScenes = request.continueAcrossScenes;
      current_.fadeOutSeconds = request.fadeOutSeconds;
      audio_.SetVolume(current_.handle, request.volume);
      if (restart) audio_.Restart(current_.handle);
      Fade(current_, 1.0f, request.fadeInSeconds);
      return;
   }
   // 新しい要求は最も古い切り替え中トラックを置き換え、短時間の連続切り替えでも2ボイスに収める。
   Release(outgoing_);
   // 現在のトラックをフェードする前に置き換え先を確保する。上限時も再生中の音楽は維持する。
   AudioHandle nextHandle = audio_.Create(request.clip, AudioBus::Bgm, request.volume, 1.0f, request.loop);
   if (!nextHandle) return;
   outgoing_ = std::move(current_);
   current_ = {};
   if (audio_.IsValid(outgoing_.handle)) Fade(outgoing_, 0.0f, outgoing_.fadeOutSeconds);
   current_.handle = nextHandle;
   current_.assetId = request.clipAssetId;
   current_.owner = owner;
   current_.continueAcrossScenes = request.continueAcrossScenes;
   current_.fadeOutSeconds = request.fadeOutSeconds;
   current_.gain = request.fadeInSeconds > 0.0f ? 0.0f : 1.0f;
   audio_.SetFade(current_.handle, current_.gain);
   Fade(current_, 1.0f, request.fadeInSeconds);
}
void BgmPlayer::ReleaseOwner(uint64_t owner) {
   if (owner && current_.owner == owner) {
      current_.owner = 0;
      if (!sceneChanging_) Fade(current_, 0.0f, current_.fadeOutSeconds);
   }
}
void BgmPlayer::EndSceneChange(bool hasNewRequest) {
   sceneChanging_ = false;
   if (!hasNewRequest && !current_.continueAcrossScenes)
      Fade(current_, 0.0f, current_.fadeOutSeconds);
}
void BgmPlayer::Update(float seconds) {
   for (Track* track : { &outgoing_, &current_ }) {
      if (!audio_.IsValid(track->handle)) { *track = {}; continue; }
      if (track->duration <= 0.0f) continue;
      track->elapsed = std::min(track->elapsed + seconds, track->duration);
      const float fraction = track->elapsed / track->duration;
      track->gain = track->start + (track->target - track->start) * fraction;
      audio_.SetFade(track->handle, track->gain);
      if (fraction >= 1.0f) {
         track->duration = 0.0f;
         if (track->target == 0.0f) Release(*track);
      }
   }
}
void BgmPlayer::StopImmediate() {
   Release(outgoing_);
   Release(current_);
   sceneChanging_ = false;
}
}
