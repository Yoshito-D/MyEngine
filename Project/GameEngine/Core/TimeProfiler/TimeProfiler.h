#pragma once
#include <chrono>

namespace GameEngine {
/// @brief 時間計測クラス
class TimeProfiler {
public:
   using Clock = std::chrono::high_resolution_clock;
   using TimePoint = std::chrono::time_point<Clock>;

   /// @brief コンストラクタ
   TimeProfiler();

   /// @brief フレームごとの更新処理
   void Update();

   /// @brief FPSを取得
   /// @return FPS値
   float GetFPS() const;

   /// @brief デルタタイムを取得
   /// @return デルタタイム値
   float GetDeltaTime() const;

   /// @brief 経過時間を取得
   /// @return 経過時間値
   float GetFrameTimeMs() const;

private:
   TimePoint prevTime_;
   float deltaTime_ = 0.0f;
   float elapsedTime_ = 0.0f;
   int frameCount_ = 0;
   float fps_ = 0.0f;
};
}
