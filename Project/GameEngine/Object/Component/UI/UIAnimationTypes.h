#pragma once

#include "Utility/MathUtils/MathConstants.h"
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
inline UIPlaybackSample EvaluateUIPlayback(
   float elapsed,
   float delay,
   float duration,
   UIPlaybackMode mode) {
   if (elapsed <= delay) {
      return {};
   }

   const float safeDuration = std::max(duration, 0.0001f);
   const float cycles = (elapsed - std::max(delay, 0.0f)) / safeDuration;
   switch (mode) {
      case UIPlaybackMode::Loop:
         return { cycles - std::floor(cycles), false };
      case UIPlaybackMode::PingPong: {
         const float phase = std::fmod(cycles, 2.0f);
         return { phase <= 1.0f ? phase : 2.0f - phase, false };
      }
      case UIPlaybackMode::Once:
      default:
         return { std::min(cycles, 1.0f), cycles >= 1.0f };
   }
}

/// @brief 0から1の進捗にイージングを適用する
/// @param progress 0から1の進捗
/// @param easing 補間曲線
/// @return 補間後の進捗
inline float EvaluateUIEasing(float progress, UIEasingType easing) {
   const float t = (std::clamp)(progress, 0.0f, 1.0f);
   switch (easing) {
      case UIEasingType::EaseInOutSine:
         return -(std::cos(MathConstants::kPi * t) - 1.0f) * 0.5f;
      case UIEasingType::EaseOutCubic: {
         const float inverse = 1.0f - t;
         return 1.0f - inverse * inverse * inverse;
      }
      case UIEasingType::EaseOutBack: {
         constexpr float kOvershoot = 1.70158f;
         const float shifted = t - 1.0f;
         return 1.0f + (kOvershoot + 1.0f) * shifted * shifted * shifted + kOvershoot * shifted * shifted;
      }
      case UIEasingType::Linear:
      default:
         return t;
   }
}

} // namespace GameEngine
