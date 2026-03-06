#pragma once
#include "../VectorMath.h"
#include <vector>

namespace GameEngine {

/// @brief Catmull-Rom補間
/// @param p0 制御点0
/// @param p1 制御点1
/// @param p2 制御点2
/// @param p3 制御点3
/// @param t 補間係数（0.0～1.0）
/// @return 補間された位置
Vector3 CatmullRomInterpolation(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t);

/// @brief Catmull-Romスプライン曲線上の位置を取得
/// @param controlPoints 制御点の配列
/// @param t 補間係数（0.0～1.0）
/// @return 補間された位置
Vector3 CatmullRomPosition(const std::vector<Vector3>& controlPoints, float t);

} // namespace GameEngine
