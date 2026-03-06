#pragma once
#include "../VectorMath.h"

namespace GameEngine {

/// @brief 座標変換
/// @param vector 変換するベクトル
/// @param matrix 変換行列
/// @return 変換後のベクトル
Vector3 TransformCoordinate(const Vector3& vector, const Matrix4x4& matrix);

/// @brief ベクトルを行列で変換
/// @param vec 変換するベクトル
/// @param m 変換行列
/// @return 変換後のベクトル
Vector4 TransformVectorByMatrix(const Vector4& vec, const Matrix4x4& m);

/// @brief 変換行列を使用して位置を変換
/// @param mat 変換行列
/// @param pos 変換する位置
/// @return 変換後の位置
Vector3 TransformPosition(const Matrix4x4& mat, const Vector3& pos);

/// @brief 法線ベクトルの変換
/// @param vector 変換する法線ベクトル
/// @param matrix 変換行列
/// @return 変換後の法線ベクトル
Vector3 TransformNormal(const Vector3& vector, const Matrix4x4& matrix);

/// @brief 2つのベクトルから法線ベクトルを計算
/// @param a ベクトルA
/// @param b ベクトルB
/// @param c ベクトルC
/// @return 計算された法線ベクトル
Vector3 ComputeNormal(const Vector3& a, const Vector3& b, const Vector3& c);

/// @brief ベクトルの正規化
/// @param vec 正規化するベクトル
/// @return 正規化されたベクトル
Vector3 Normalize(const Vector3& vec);

/// @brief ワールド座標をスクリーン座標に投影
/// @param worldPosition ワールド座標
/// @param viewportX ビューポートX座標
/// @param viewportY ビューポートY座標
/// @param viewportWidth ビューポート幅
/// @param viewportHeight ビューポート高さ
/// @param viewProjectionMatrix ビュープロジェクション行列
/// @return スクリーン座標
Vector3 Project(const Vector3& worldPosition, float viewportX, float viewportY, float viewportWidth, float viewportHeight, const Matrix4x4& viewProjectionMatrix);

} // namespace GameEngine
