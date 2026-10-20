#pragma once

#include "GameEngine/Math/Functions/MathConstants.h"
#include <algorithm>
#include <cmath>

namespace GameEngine {

/// @brief UIアニメーションの繰り返し方法
enum class UIPlaybackMode {
   Once,
   Loop,
   PingPong
};

/// @brief UIアニメーションの補間曲線
enum class UIEasingType {
   Linear,
   EaseInOutSine,
   EaseOutCubic,
   EaseOutBack
};

/// @brief 再生時間から得られる正規化進捗
struct UIPlaybackSample {
   float progress = 0.0f;
   bool finished = false;
};

/// @brief 再生時間を0から1の進捗に変換する
/// @param elapsed 再生開始後の経過秒
/// @param delay 開始待ち秒
/// @param duration 片道の再生秒
/// @param mode 繰り返し方法
/// @return 正規化進捗と完了状態
UIPlaybackSample EvaluateUIPlayback(float elapsed, float delay, float duration, UIPlaybackMode mode);

/// @brief 0から1の進捗にイージングを適用する
/// @param progress 0から1の進捗
/// @param easing 補間曲線
/// @return 補間後の進捗
float EvaluateUIEasing(float progress, UIEasingType easing);

} // namespace GameEngine
