#include "CameraKeyframe.h"
#include "Utility/MathUtils.h"
#include <algorithm>
#include <cmath>

CameraKeyframe CameraKeyframe::Interpolate(const CameraKeyframe& start, const CameraKeyframe& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   
   // イージングを適用
   float easedT = ApplyEasing(t, end.easingType, end.easingPower);

   CameraKeyframe result;
   
   // 位置の補間
   result.position.x = start.position.x + (end.position.x - start.position.x) * easedT;
   result.position.y = start.position.y + (end.position.y - start.position.y) * easedT;
   result.position.z = start.position.z + (end.position.z - start.position.z) * easedT;

   // 回転の補間（Euler角の線形補間）
   result.rotation.x = start.rotation.x + (end.rotation.x - start.rotation.x) * easedT;
   result.rotation.y = start.rotation.y + (end.rotation.y - start.rotation.y) * easedT;
   result.rotation.z = start.rotation.z + (end.rotation.z - start.rotation.z) * easedT;

   // FOVの補間
   result.fov = start.fov + (end.fov - start.fov) * easedT;

   // その他のパラメータは終了キーフレームのものを使用
   result.duration = end.duration;
   result.easingType = end.easingType;
   result.easingPower = end.easingPower;
   result.useFade = end.useFade;
   result.fadeDuration = end.fadeDuration;
   result.fadeColor = end.fadeColor;

   return result;
}

float CameraKeyframe::ApplyEasing(float t, EasingType type, float power) {
   switch (type) {
	  case EasingType::Linear:
		 return t;

	  case EasingType::EaseIn:
		 return std::pow(t, power);

	  case EasingType::EaseOut:
		 return 1.0f - std::pow(1.0f - t, power);

	  case EasingType::EaseInOut:
		 if (t < 0.5f) {
			return std::pow(t * 2.0f, power) * 0.5f;
		 } else {
			return 1.0f - std::pow((1.0f - t) * 2.0f, power) * 0.5f;
		 }

	  case EasingType::Bounce: {
		 if (t < 1.0f / 2.75f) {
			return 7.5625f * t * t;
		 } else if (t < 2.0f / 2.75f) {
			t -= 1.5f / 2.75f;
			return 7.5625f * t * t + 0.75f;
		 } else if (t < 2.5f / 2.75f) {
			t -= 2.25f / 2.75f;
			return 7.5625f * t * t + 0.9375f;
		 } else {
			t -= 2.625f / 2.75f;
			return 7.5625f * t * t + 0.984375f;
		 }
	  }

	  case EasingType::EaseOutBack: {
		 float overshoot = 1.70158f;
		 float x = t - 1.0f;
		 return x * x * ((overshoot + 1.0f) * x + overshoot) + 1.0f;
	  }

	  default:
		 return t;
   }
}
