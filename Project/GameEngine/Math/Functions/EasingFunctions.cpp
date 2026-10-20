#include "GameEngine/pch.h"
#include "GameEngine/Math/Functions/EasingFunctions.h"

namespace GameEngine {

Vector3 Easing::Slerp(const Vector3& start, const Vector3& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float dot = std::clamp(start.Dot(end), -1.0f, 1.0f);
   float theta = acosf(dot);
   float sinTheta = std::sin(theta);

   if (std::abs(sinTheta) < 1e-5f) {
      // ほぼ平行な方向では除算が不安定になるため、線形補間してから正規化する。
      return (start * (1.0f - t) + end * t).Normalize();
   }

   return start * std::sin((1.0f - t) * theta) / sinTheta + end * std::sin(t * theta) / sinTheta;
}

} // namespace GameEngine

namespace GameEngine::Easing {

float BounceOut(float t) {
   const float n1 = 7.5625f;
   const float d1 = 2.75f;

   if (t < 1.0f / d1) {
      return n1 * t * t;
   } else if (t < 2.0f / d1) {
      t -= 1.5f / d1;
      return n1 * t * t + 0.75f;
   } else if (t < 2.5f / d1) {
      t -= 2.25f / d1;
      return n1 * t * t + 0.9375f;
   } else {
      t -= 2.625f / d1;
      return n1 * t * t + 0.984375f;
   }
}

} // namespace GameEngine::Easing
