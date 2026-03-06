#pragma once
#include "../VectorMath.h"

namespace GameEngine {

/// @brief ディグリーからラジアンに変換
/// @param degrees 角度（度）
/// @return 角度（ラジアン）
float ToRadians(float degrees);

/// @brief ラジアンからディグリーに変換
/// @param radians 角度（ラジアン）
/// @return 角度（度）
float ToDegrees(float radians);

/// @brief 単位行列の生成
/// @return 単位行列
Matrix4x4 MakeIdentity4x4();

/// @brief 単位行列の生成
/// @return 単位行列
Matrix3x3 MakeIdentity3x3();

/// @brief トランスフォームの初期化
/// @return トランスフォーム
Transform TransformInitialize();

/// @brief スケール行列の生成
/// @param scale スケール
/// @return スケール行列
Matrix4x4 MakeScaleMatrix(const Vector3& scale);

/// @brief 平行移動行列の生成
/// @param translate 平行移動ベクトル
/// @return 平行移動行列
Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

/// @brief 回転行列X軸の生成
/// @param radian 角度（ラジアン）
/// @return 回転行列X軸
Matrix4x4 MakeRotateXMatrix(float radian);

/// @brief 回転行列Y軸の生成
/// @param radian 角度（ラジアン）
/// @return 回転行列Y軸
Matrix4x4 MakeRotateYMatrix(float radian);

/// @brief 回転行列Z軸の生成
/// @param radian 角度（ラジアン）
/// @return 回転行列Z軸
Matrix4x4 MakeRotateZMatrix(float radian);

/// @brief アフィン変換行列の生成
/// @param transform トランスフォーム
/// @return アフィン変換行列
Matrix4x4 MakeAffineMatrix(const Transform& transform);

/// @brief 任意軸周りの回転行列を生成
/// @param axis 回転軸
/// @param angle 回転角度（ラジアン）
/// @return 回転行列
Matrix4x4 MakeRotationAxis(const Vector3& axis, float angle);

/// @brief 任意軸周りの回転行列を生成（Matrix4x4版）
/// @param axis 回転軸
/// @param angle 回転角度（ラジアン）
/// @return 回転行列
Matrix4x4 MakeRotateAxisAngle(const Vector3& axis, float angle);

/// @brief 2つのベクトル間の回転行列を生成
/// @param from 開始方向ベクトル
/// @param to 終了方向ベクトル
/// @return 回転行列
Matrix4x4 MakeRotationMatrixFromTo(const Vector3& from, const Vector3& to);

/// @brief 2つの方向ベクトル間の回転行列を生成
/// @param from 開始方向ベクトル
/// @param to 終了方向ベクトル
/// @return 回転行列
Matrix4x4 DirectionToDirection(const Vector3& from, const Vector3& to);

/// @brief 透視投影行列の生成
/// @param fovY 垂直方向の視野角（ラジアン）
/// @param aspectRatio アスペクト比（幅/高さ）
/// @param nearClip 近クリップ面の距離
/// @param farClip 遠クリップ面の距離
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

/// @brief 平行投影行列の生成
/// @param left 左端の座標
/// @param top 上端の座標
/// @param right 右端の座標
/// @param bottom 下端の座標
/// @param nearClip 近クリップ面の距離
/// @param farClip 遠クリップ面の距離
/// @return 平行投影行列
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

/// @brief ビューポート変換行列の生成
/// @param left ビューポートの左端の座標
/// @param top ビューポートの上端の座標
/// @param width ビューポートの幅
/// @param height ビューポートの高さ
/// @param minDepth ビューポートの最小深度
/// @param maxDepth ビューポートの最大深度
/// @return ビューポート変換行列
Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);

/// @brief LookAt行列の生成
/// @param eye 視点の位置
/// @param target 注視点の位置
/// @param up 上方向のベクトル
/// @return LookAt行列
Matrix4x4 MakeLookAtMatrix(const Vector3& eye, const Vector3& target, const Vector3& up);

/// @brief ワールド行列から回転行列のみを抽出
/// @param worldMatrix ワールド行列
/// @return 回転行列のみを含む行列
Matrix4x4 ExtractRotationMatrix(const Matrix4x4& worldMatrix);

/// @brief 行列をXYZ順のオイラー角に変換
/// @param m 変換する行列
/// @return オイラー角（X, Y, Z）
Vector3 MatrixToEulerXYZ(const Matrix4x4& m);

/// @brief 行列からYaw-Pitch-Rollを抽出
/// @param m 変換する行列
/// @return ヨー、ピッチ、ロール角
Vector3 ExtractYawPitchRoll(const Matrix4x4& m);

} // namespace GameEngine
