#include "GameEngine/pch.h"
#include "GameEngine/Object/Component/UI/UIAnimationTypes.h"

namespace GameEngine {

UIPlaybackSample EvaluateUIPlayback(float elapsed, float delay, float duration, UIPlaybackMode mode) {
   if (elapsed <= delay) {
      return {};
   }

   const float safeDuration = std::max(duration, 0.0001f);
   const float cycles = (elapsed - std::max(delay, 0.0f)) / safeDuration;

   switch (mode) {
      case UIPlaybackMode::Loop:
         return {cycles - std::floor(cycles), false};
      case UIPlaybackMode::PingPong: {
         const float phase = std::fmod(cycles, 2.0f);
         return {phase <= 1.0f ? phase : 2.0f - phase, false};
      }
      case UIPlaybackMode::Once:
      default:
         return {std::min(cycles, 1.0f), cycles >= 1.0f};
   }
}

float EvaluateUIEasing(float progress, UIEasingType easing) {
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
