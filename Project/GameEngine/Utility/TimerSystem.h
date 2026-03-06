#pragma once
#include <functional>
#include <unordered_map>
#include <cstdint>
#include <vector>
#include <optional>
#include "Framework/EngineContext.h"

namespace GameEngine {

/// @brief フレーム更新に同期して動作する簡易タイマーシステム
/// 使用方法:
/// - 1) タイマー登録: TimerSystem::SetTimeout / SetInterval
/// - 2) 毎フレームどこかで TimerSystem::Tick() もしくは Update(deltaTime) を呼ぶ
class TimerSystem {
public:
   using TimerId = uint64_t;

   /// @brief 1回だけ呼ばれるタイマーを設定
   /// @param delaySec 遅延秒
   /// @param callback コールバック
   /// @return タイマーID
   static TimerId SetTimeout(float delaySec, std::function<void()> callback);

   /// @brief 周期的に呼ばれるタイマーを設定
   /// @param intervalSec 間隔秒
   /// @param callback コールバック
   /// @param repeat 繰り返し回数。-1 は無限
   /// @return タイマーID
   static TimerId SetInterval(float intervalSec, std::function<void()> callback, int repeat = -1);

   /// @brief タイマーを解除
   static void Clear(TimerId id);

   /// @brief すべてのタイマーを解除
   static void ClearAll();

   /// @brief 一時停止
   static void Pause(TimerId id);

   /// @brief 再開
   static void Resume(TimerId id);

   /// @brief 登録済みか
   static bool Exists(TimerId id);

   /// @brief 更新（デルタタイムを引数で渡す）
   static void Update(float deltaTime);

   /// @brief 簡易更新（EngineContext から delta を取得）
   static void Tick();

private:
   struct Timer {
	  TimerId id{};
	  float remaining{};        // 次発火までの残り時間
	  float interval{};         // 間隔（0 の場合は一度きり）
	  int repeat{ -1 };           // 残り回数（-1 は無限）
	  bool paused{ false };
	  std::function<void()> callback{};
   };

   static TimerId NextId();
   static std::unordered_map<TimerId, Timer>& Timers();
};
}
