#include "TimeProfiler.h"

namespace GameEngine {
TimeProfiler::TimeProfiler() {
   prevTime_ = Clock::now();
}

void TimeProfiler::Update() {
   // 前フレームの計測点との差だけを秒へ変換し、次回基準を同じ時刻へ更新する。
   TimePoint currentTime = Clock::now();
   std::chrono::duration<float> delta = currentTime - prevTime_;
   deltaTime_ = delta.count(); // 秒単位
   prevTime_ = currentTime;

   // FPSカウント
   frameCount_++;
   elapsedTime_ += deltaTime_;

   if (elapsedTime_ >= 1.0f) {
	  // ちょうど1秒とは限らないため、実際の累積時間で割ってフレーム落ち時の偏りを抑える。
	  fps_ = static_cast<float>(frameCount_) / elapsedTime_;
	  frameCount_ = 0;
	  elapsedTime_ = 0.0f;
   }
}

float TimeProfiler::GetFPS() const { return fps_; }

float TimeProfiler::GetDeltaTime() const { return deltaTime_; }

float TimeProfiler::GetFrameTimeMs() const { return deltaTime_ * 1000.0f; }
}