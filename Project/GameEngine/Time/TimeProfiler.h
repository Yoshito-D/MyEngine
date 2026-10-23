#pragma once

#include <cstdint>
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
   TimePoint prevTime_;          ///< 前回の実フレーム時間を計測した時刻。
   float deltaTime_ = 0.0f;      ///< 現在フレームで進める時間（秒）。
   float elapsedTime_ = 0.0f;    ///< 現在の処理区間で累積した経過時間（秒）。
   std::int32_t frameCount_ = 0; ///< 現在の計測区間で数えたフレーム数。
   float fps_ = 0.0f;            ///< 直近の計測区間の平均フレームレート。
};
}
