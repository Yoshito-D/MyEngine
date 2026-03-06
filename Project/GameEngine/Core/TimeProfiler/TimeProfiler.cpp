#include "TimeProfiler.h"

namespace GameEngine {
TimeProfiler::TimeProfiler() {
   prevTime_ = Clock::now();
}

void TimeProfiler::Update() {
   TimePoint currentTime = Clock::now();
   std::chrono::duration<float> delta = currentTime - prevTime_;
   deltaTime_ = delta.count(); // 秒単位
   prevTime_ = currentTime;

   // FPSカウント
   frameCount_++;
   elapsedTime_ += deltaTime_;

   if (elapsedTime_ >= 1.0f) {
	  fps_ = static_cast<float>(frameCount_) / elapsedTime_;
	  frameCount_ = 0;
	  elapsedTime_ = 0.0f;
   }
}

float TimeProfiler::GetFPS() const { return fps_; }

float TimeProfiler::GetDeltaTime() const { return deltaTime_; }

float TimeProfiler::GetFrameTimeMs() const { return deltaTime_ * 1000.0f; }
}