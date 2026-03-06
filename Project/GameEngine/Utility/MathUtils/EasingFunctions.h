#pragma once
#include "../VectorMath.h"
#include <algorithm>
#include <concepts>
#include <cstdint>
#include <functional>

namespace GameEngine {
namespace Easing {

template<typename T>
concept Interpolatable = requires(T a, T b, float t) {
   { b - a } -> std::convertible_to<T>;
   { (b - a)* t } -> std::convertible_to<T>;
   { a + ((b - a) * t) } -> std::convertible_to<T>;
};

/// @brief イージング関数の型
template<typename T>
using EasingFunc = std::function<T(const T&, const T&, float)>;

// ===== 基本的な補間関数 =====

/// @brief 2つの値の間を線形補間します（テンプレート版、数値型用）
/// @tparam T 補間する値の型。算術型である必要があります
/// @param start 補間の開始値
/// @param end 補間の終了値
/// @param t 補間係数（通常0.0から1.0の範囲）
/// @return startとendの間をtで線形補間した値
template<Interpolatable T>
constexpr T Lerp(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   return static_cast<T>(start + (end - start) * t);
}

/// @brief 球面線形補間（Vector3用）
/// @param start 開始ベクトル
/// @param end 終了ベクトル
/// @param t 補間係数（0.0～1.0）
/// @return 補間されたベクトル
Vector3 Slerp(const Vector3& start, const Vector3& end, float t);

// ===== Sine系イージング =====

/// @brief サインイーズイン
template<Interpolatable T>
constexpr T EaseInSine(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = 1.0f - std::cos((t * 3.14159265358979323846f) / 2.0f);
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief サインイーズアウト
template<Interpolatable T>
constexpr T EaseOutSine(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = std::sin((t * 3.14159265358979323846f) / 2.0f);
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief サインイーズインアウト
template<Interpolatable T>
constexpr T EaseInOutSine(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = -(std::cos(3.14159265358979323846f * t) - 1.0f) / 2.0f;
   return static_cast<T>(start + (end - start) * easedT);
}

// ===== Quad系イージング（2乗） =====

/// @brief 2次関数イーズイン
template<Interpolatable T>
constexpr T EaseInQuad(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = t * t;
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief 2次関数イーズアウト
template<Interpolatable T>
constexpr T EaseOutQuad(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = 1.0f - (1.0f - t) * (1.0f - t);
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief 2次関数イーズインアウト
template<Interpolatable T>
constexpr T EaseInOutQuad(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
   return static_cast<T>(start + (end - start) * easedT);
}

// ===== Cubic系イージング（3乗） =====

/// @brief 3次関数イーズイン
template<Interpolatable T>
constexpr T EaseInCubic(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = t * t * t;
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief 3次関数イーズアウト
template<Interpolatable T>
constexpr T EaseOutCubic(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = 1.0f - std::pow(1.0f - t, 3.0f);
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief 3次関数イーズインアウト
template<Interpolatable T>
constexpr T EaseInOutCubic(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
   return static_cast<T>(start + (end - start) * easedT);
}

// ===== Quart系イージング（4乗） =====

/// @brief 4次関数イーズイン
template<Interpolatable T>
constexpr T EaseInQuart(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = t * t * t * t;
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief 4次関数イーズアウト
template<Interpolatable T>
constexpr T EaseOutQuart(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = 1.0f - std::pow(1.0f - t, 4.0f);
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief 4次関数イーズインアウト
template<Interpolatable T>
constexpr T EaseInOutQuart(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = t < 0.5f ? 8.0f * t * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 4.0f) / 2.0f;
   return static_cast<T>(start + (end - start) * easedT);
}

// ===== Quint系イージング（5乗） =====

/// @brief 5次関数イーズイン
template<Interpolatable T>
constexpr T EaseInQuint(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = t * t * t * t * t;
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief 5次関数イーズアウト
template<Interpolatable T>
constexpr T EaseOutQuint(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = 1.0f - std::pow(1.0f - t, 5.0f);
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief 5次関数イーズインアウト
template<Interpolatable T>
constexpr T EaseInOutQuint(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = t < 0.5f ? 16.0f * t * t * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 5.0f) / 2.0f;
   return static_cast<T>(start + (end - start) * easedT);
}

// ===== Expo系イージング（指数関数） =====

/// @brief 指数関数イーズイン
template<Interpolatable T>
constexpr T EaseInExpo(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = t == 0.0f ? 0.0f : std::pow(2.0f, 10.0f * t - 10.0f);
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief 指数関数イーズアウト
template<Interpolatable T>
constexpr T EaseOutExpo(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = t == 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief 指数関数イーズインアウト
template<Interpolatable T>
constexpr T EaseInOutExpo(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT;
   if (t == 0.0f) {
      easedT = 0.0f;
   } else if (t == 1.0f) {
      easedT = 1.0f;
   } else if (t < 0.5f) {
      easedT = std::pow(2.0f, 20.0f * t - 10.0f) / 2.0f;
   } else {
      easedT = (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) / 2.0f;
   }
   return static_cast<T>(start + (end - start) * easedT);
}

// ===== Circ系イージング（円形） =====

/// @brief 円形イーズイン
template<Interpolatable T>
constexpr T EaseInCirc(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = 1.0f - std::sqrt(1.0f - t * t);
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief 円形イーズアウト
template<Interpolatable T>
constexpr T EaseOutCirc(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = std::sqrt(1.0f - std::pow(t - 1.0f, 2.0f));
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief 円形イーズインアウト
template<Interpolatable T>
constexpr T EaseInOutCirc(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = t < 0.5f
      ? (1.0f - std::sqrt(1.0f - std::pow(2.0f * t, 2.0f))) / 2.0f
      : (std::sqrt(1.0f - std::pow(-2.0f * t + 2.0f, 2.0f)) + 1.0f) / 2.0f;
   return static_cast<T>(start + (end - start) * easedT);
}

// ===== Back系イージング（バックオーバーシュート） =====

/// @brief バックイーズイン
template<Interpolatable T>
constexpr T EaseInBack(const T& start, const T& end, float t, float overshoot = 1.70158f) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = (overshoot + 1.0f) * t * t * t - overshoot * t * t;
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief バックイーズアウト
template<Interpolatable T>
constexpr T EaseOutBack(T start, T end, float t, float overshoot = 1.70158f) {
   t = std::clamp(t, 0.0f, 1.0f);
   float x = t - 1.0f;
   float easedT = x * x * ((overshoot + 1.0f) * x + overshoot) + 1.0f;
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief バックイーズインアウト
template<Interpolatable T>
constexpr T EaseInOutBack(const T& start, const T& end, float t, float overshoot = 1.70158f) {
   t = std::clamp(t, 0.0f, 1.0f);
   float c2 = overshoot * 1.525f;
   float easedT;
   if (t < 0.5f) {
      easedT = (std::pow(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f;
   } else {
      float x = 2.0f * t - 2.0f;
      easedT = (std::pow(x, 2.0f) * ((c2 + 1.0f) * x + c2) + 2.0f) / 2.0f;
   }
   return static_cast<T>(start + (end - start) * easedT);
}

// ===== Elastic系イージング（弾性） =====

/// @brief エラスティックイーズイン
template<Interpolatable T>
constexpr T EaseInElastic(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   const float c4 = (2.0f * 3.14159265358979323846f) / 3.0f;
   float easedT;
   if (t == 0.0f) {
      easedT = 0.0f;
   } else if (t == 1.0f) {
      easedT = 1.0f;
   } else {
      easedT = -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * c4);
   }
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief エラスティックイーズアウト
template<Interpolatable T>
constexpr T EaseOutElastic(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   const float c4 = (2.0f * 3.14159265358979323846f) / 3.0f;
   float easedT;
   if (t == 0.0f) {
      easedT = 0.0f;
   } else if (t == 1.0f) {
      easedT = 1.0f;
   } else {
      easedT = std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
   }
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief エラスティックイーズインアウト
template<Interpolatable T>
constexpr T EaseInOutElastic(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   const float c5 = (2.0f * 3.14159265358979323846f) / 4.5f;
   float easedT;
   if (t == 0.0f) {
      easedT = 0.0f;
   } else if (t == 1.0f) {
      easedT = 1.0f;
   } else if (t < 0.5f) {
      easedT = -(std::pow(2.0f, 20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f;
   } else {
      easedT = (std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f + 1.0f;
   }
   return static_cast<T>(start + (end - start) * easedT);
}

// ===== Bounce系イージング（バウンス） =====

/// @brief バウンスイーズアウト（ヘルパー関数）
inline float BounceOut(float t) {
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

/// @brief バウンスイーズイン
template<Interpolatable T>
constexpr T EaseInBounce(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = 1.0f - BounceOut(1.0f - t);
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief バウンスイーズアウト
template<Interpolatable T>
constexpr T EaseOutBounce(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = BounceOut(t);
   return static_cast<T>(start + (end - start) * easedT);
}

/// @brief バウンスイーズインアウト
template<Interpolatable T>
constexpr T EaseInOutBounce(const T& start, const T& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float easedT = t < 0.5f
      ? (1.0f - BounceOut(1.0f - 2.0f * t)) / 2.0f
      : (1.0f + BounceOut(2.0f * t - 1.0f)) / 2.0f;
   return static_cast<T>(start + (end - start) * easedT);
}

// ===== レガシー関数（後方互換性） =====

/// @brief 開始値から終了値まで、指定したべき乗（power）でイーズイン・イーズアウト補間を行います
/// @deprecated EaseInOutQuad, EaseInOutCubic などを使用してください
template<Interpolatable T>
constexpr T EaseInOut(T start, T end, float t, float power) {
   t = std::clamp(t, 0.0f, 1.0f);

   if (t < 0.5f) {
	  float scaledT = std::pow(t * 2.0f, power) * 0.5f;
	  return static_cast<T>(start + (end - start) * scaledT);
   } else {
	  float scaledT = 1.0f - std::pow((1.0f - t) * 2.0f, power) * 0.5f;
	  return static_cast<T>(start + (end - start) * scaledT);
   }
}

/// @brief 開始値から終了値まで、指定したべき乗（power）でイーズアウト補間する
/// @deprecated EaseOutQuad, EaseOutCubic などを使用してください
template<Interpolatable T>
constexpr T EaseOut(T start, T end, float t, float power) {
   t = std::clamp(t, 0.0f, 1.0f);
   float scaledT = 1.0f - std::pow(1.0f - t, power);
   return static_cast<T>(start + (end - start) * scaledT);
}

/// @brief 開始値から終了値まで、指定したべき乗（power）でイーズイン補間する
/// @deprecated EaseInQuad, EaseInCubic などを使用してください
template<Interpolatable T>
constexpr T EaseIn(T start, T end, float t, float power) {
   t = std::clamp(t, 0.0f, 1.0f);
   float scaledT = std::pow(t, power);
   return static_cast<T>(start + (end - start) * scaledT);
}

// ===== 複合イージング機能 =====

/// @brief 2つのイージング関数を組み合わせる（tの中間点で切り替え）
/// @tparam T 補間する値の型
/// @param start 補間の開始値
/// @param end 補間の終了値
/// @param t 補間係数（0.0～1.0）
/// @param firstEasing 前半で使用するイージング関数
/// @param secondEasing 後半で使用するイージング関数
/// @param splitPoint 切り替え点（デフォルト0.5）
/// @return 複合イージングで補間された値
template<Interpolatable T>
constexpr T CompoundEasing(
   const T& start,
   const T& end,
   float t,
   EasingFunc<T> firstEasing,
   EasingFunc<T> secondEasing,
   float splitPoint = 0.5f
) {
   t = std::clamp(t, 0.0f, 1.0f);
   splitPoint = std::clamp(splitPoint, 0.0f, 1.0f);
   
   if (t < splitPoint) {
      // 前半: 0.0 ～ splitPoint を 0.0 ～ 1.0 にマッピング
      float normalizedT = t / splitPoint;
      T midPoint = static_cast<T>(start + (end - start) * splitPoint);
      return firstEasing(start, midPoint, normalizedT);
   } else {
      // 後半: splitPoint ～ 1.0 を 0.0 ～ 1.0 にマッピング
      float normalizedT = (t - splitPoint) / (1.0f - splitPoint);
      T midPoint = static_cast<T>(start + (end - start) * splitPoint);
      return secondEasing(midPoint, end, normalizedT);
   }
}

/// @brief 2つのRGBAカラーを線形補間する
/// @param colorA 最初のRGBAカラー
/// @param colorB 2番目のRGBAカラー
/// @param t 補間係数（0.0〜1.0の範囲）
/// @return 線形補間されたRGBAカラー（ARGB形式の32ビット整数）
uint32_t LerpRGBAColor(uint32_t colorA, uint32_t colorB, float t);

} // namespace Easing
} // namespace GameEngine
