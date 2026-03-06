#include "CameraSequencer.h"
#include "Framework/EngineContext.h"
#include <algorithm>

CameraSequencer::CameraSequencer(GameEngine::Camera* camera) 
   : camera_(camera) {
}

void CameraSequencer::AddKeyframe(const CameraKeyframe& keyframe) {
   keyframes_.push_back(keyframe);
   CalculateTotalDuration();
}

void CameraSequencer::SetKeyframes(const std::vector<CameraKeyframe>& keyframes) {
   keyframes_ = keyframes;
   CalculateTotalDuration();
}

void CameraSequencer::Play(bool loop) {
   if (keyframes_.empty()) {
	  return;
   }

   state_ = SequenceState::Playing;
   isLooping_ = loop;
   currentKeyframeIndex_ = 0;
   currentTime_ = 0.0f;
   keyframeStartTime_ = 0.0f;

   // 最初のキーフレームを適用
   if (!keyframes_.empty()) {
	  ApplyCameraKeyframe(keyframes_[0]);
	  
	  if (onKeyframeChangeCallback_) {
		 onKeyframeChangeCallback_(0);
	  }
   }
}

void CameraSequencer::Pause() {
   if (state_ == SequenceState::Playing) {
	  state_ = SequenceState::Paused;
   }
}

void CameraSequencer::Resume() {
   if (state_ == SequenceState::Paused) {
	  state_ = SequenceState::Playing;
   }
}

void CameraSequencer::Stop() {
   state_ = SequenceState::Idle;
   currentKeyframeIndex_ = 0;
   currentTime_ = 0.0f;
   keyframeStartTime_ = 0.0f;
   isFading_ = false;
   sceneFade_.reset();
}

void CameraSequencer::Reset() {
   Stop();
   keyframes_.clear();
   totalDuration_ = 0.0f;
}

void CameraSequencer::Update() {
   // フェードの更新
   if (sceneFade_) {
	  sceneFade_->Update();
   }

   if (state_ != SequenceState::Playing || keyframes_.empty()) {
	  return;
   }

   float deltaTime = GameEngine::EngineContext::GetDeltaTime();
   currentTime_ += deltaTime;

   // 現在のキーフレームの範囲を計算
   float currentKeyframeDuration = keyframes_[currentKeyframeIndex_].duration;
   float timeInKeyframe = currentTime_ - keyframeStartTime_;

   // キーフレーム完了の少し前（フェード時間分）でフェードアウトを開始
   const auto& currentKeyframe = keyframes_[currentKeyframeIndex_];
   if (currentKeyframe.useFade && !isFading_) {
	  // フェード開始タイミング = キーフレーム終了の fadeDuration 秒前
	  float fadeStartTime = currentKeyframeDuration - currentKeyframe.fadeDuration;
	  if (timeInKeyframe >= fadeStartTime) {
		 // フェード開始
		 if (!sceneFade_) {
			StartFade(currentKeyframe);
		 }
		 // フェードアウト完了フラグをリセット
		 sceneFade_->ResetFadeOutCompleted();
		 sceneFade_->StartFadeOut();
		 isFading_ = true;
	  }
   }

   // キーフレーム間の補間
   if (currentKeyframeIndex_ < keyframes_.size() && timeInKeyframe < currentKeyframeDuration) {
	  float t = timeInKeyframe / currentKeyframeDuration;
	  
	  // 前のキーフレームと現在のキーフレーム間を補間
	  if (currentKeyframeIndex_ == 0) {
		 // 最初のキーフレームはそのまま適用
		 ApplyCameraKeyframe(keyframes_[0]);
	  } else {
		 CameraKeyframe interpolated = CameraKeyframe::Interpolate(
			keyframes_[currentKeyframeIndex_ - 1],
			keyframes_[currentKeyframeIndex_],
			t
		 );
		 ApplyCameraKeyframe(interpolated);
	  }
   } else {
	  // フェードアウトが完了するまで待機
	  if (isFading_ && sceneFade_ && !sceneFade_->IsFadeOutCompleted()) {
		 return;  // フェードアウト中は次のキーフレームに進まない
	  }

	  // 次のキーフレームへ移行
	  currentKeyframeIndex_++;

	  if (currentKeyframeIndex_ >= keyframes_.size()) {
		 // シーケンス完了
		 if (isLooping_) {
			// ループ再生
			currentKeyframeIndex_ = 0;
			currentTime_ = 0.0f;
			keyframeStartTime_ = 0.0f;
			isFading_ = false;
			
			if (onKeyframeChangeCallback_) {
			   onKeyframeChangeCallback_(0);
			}
		 } else {
			// 完了
			state_ = SequenceState::Completed;
			if (onCompleteCallback_) {
			   onCompleteCallback_();
			}
		 }
	  } else {
		 // 次のキーフレームの開始時間を設定
		 keyframeStartTime_ = currentTime_;
		 
		 // 次のキーフレームでフェードインを開始
		 if (isFading_ && sceneFade_) {
			sceneFade_->StartFadeIn();
		 }
		 isFading_ = false;  // 次のキーフレーム用にリセット

		 // コールバック呼び出し
		 if (onKeyframeChangeCallback_) {
			onKeyframeChangeCallback_(currentKeyframeIndex_);
		 }
	  }
   }
}

void CameraSequencer::DrawFade() {
   if (sceneFade_) {
	  sceneFade_->Draw();
   }
}

void CameraSequencer::ApplyCameraKeyframe(const CameraKeyframe& keyframe) {
   if (!camera_) {
	  return;
   }

   camera_->SetPosition(keyframe.position);
   camera_->SetRotation(keyframe.rotation);
   camera_->SetFovY(keyframe.fov);
   camera_->Update();
}

void CameraSequencer::CalculateTotalDuration() {
   totalDuration_ = 0.0f;
   for (const auto& keyframe : keyframes_) {
	  totalDuration_ += keyframe.duration;
   }
}

void CameraSequencer::StartFade(const CameraKeyframe& keyframe) {
   if (!sceneFade_) {
	  sceneFade_ = std::make_unique<GameEngine::SceneFade>();
	  sceneFade_->Initialize(keyframe.fadeDuration, keyframe.fadeColor);
   } else {
	  sceneFade_->SetFadeDuration(keyframe.fadeDuration);
	  sceneFade_->SetFadeColor(keyframe.fadeColor);
   }
   isFading_ = true;
}
