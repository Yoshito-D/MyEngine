#pragma once

namespace GameEngine::MathConstants {

/// @brief 単精度浮動小数点数の円周率
inline constexpr float kPi = 3.14159265358979323846f;

/// @brief 単精度浮動小数点数の2π
inline constexpr float kTwoPi = 2.0f * kPi;

/// @brief 単精度浮動小数点数のπ/2
inline constexpr float kHalfPi = 0.5f * kPi;

/// @brief 度数法の角度をラジアンへ変換する係数
inline constexpr float kDegreesToRadians = kPi / 180.0f;

/// @brief ラジアンの角度を度数法へ変換する係数
inline constexpr float kRadiansToDegrees = 180.0f / kPi;

} // namespace GameEngine::MathConstants
