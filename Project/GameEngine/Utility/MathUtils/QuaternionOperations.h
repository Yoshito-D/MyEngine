#pragma once
#include "../VectorMath.h"

namespace GameEngine {

/// @brief 任意軸周りの回転クォータニオンを生成
/// @param axis 回転軸
/// @param angle 回転角度（ラジアン）
/// @return 回転クォータニオン
Quaternion MakeRotateAxisAngleQuaternion(const Vector3& axis, float angle);

/// @brief ベクトルをクォータニオンで回転
/// @param vector 回転するベクトル
/// @param quaternion 回転クォータニオン
/// @return 回転後のベクトル
Vector3 RotateVector(const Vector3& vector, const Quaternion& quaternion);

/// @brief クォータニオンから回転行列を生成
/// @param quaternion 回転クォータニオン
/// @return 回転行列
Matrix4x4 MakeRotateMatrix(const Quaternion& quaternion);

/// @brief 回転行列からクォータニオンを生成
/// @param matrix 回転行列
/// @return クォータニオン
Quaternion MatrixToQuaternion(const Matrix4x4& matrix);

/// @brief 2つのクォータニオン間を球面線形補間
/// @param q0 開始クォータニオン
/// @param q1 終了クォータニオン
/// @param t 補間係数（0.0～1.0）
/// @return 補間されたクォータニオン
Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t);

/// @brief ベクター3をクオータニオンに変換
/// @param eulerAngles オイラー角
/// @return クォータニオン
Quaternion Vector3ToQuaternion(const Vector3& eulerAngles);

/// @brief 前方ベクトルと上方ベクトルから回転を生成
/// @param forward 前方ベクトル
/// @param up 上方ベクトル
/// @return クォータニオン
Quaternion LookRotation(const Vector3& forward, const Vector3& up);

/// @brief 前方ベクトルから回転を生成（上方向はデフォルト）
/// @param forward 前方ベクトル
/// @return クォータニオン
Quaternion LookRotation(const Vector3& forward);

} // namespace GameEngine
