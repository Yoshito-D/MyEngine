#pragma once

#include "GameEngine/Math/Types/Vector3.h"
#include "GameEngine/Math/Types/Quaternion.h"

namespace GameEngine {
/// @brief 回転したOBBの指定方向への支持半径を計算する。
/// @param rotation OBBの向き。
/// @param halfExtents 各軸の半サイズ。符号は無視する。
/// @param direction 支持半径を求める単位方向。
float ComputeObbSupportRadius(const Quaternion& rotation, const Vector3& halfExtents, const Vector3& direction);
}
