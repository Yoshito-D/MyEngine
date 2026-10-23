#pragma once

#include "GameEngine/Math/VectorMath.h"
#include "GameEngine/Math/MathUtils.h"
#include <cmath>

namespace GameEngine {

/// @brief カメラの状態を表す値オブジェクト
struct CameraState {
   Transform transform;      ///< 位置・回転・拡縮をまとめた変換情報。
   float fov = 0.45f;        ///< 透視投影の垂直視野角（ラジアン）。
   float nearClip = 0.01f;   ///< 影用カメラの近クリップ距離。
   float farClip = 10000.0f; ///< プレイヤーと地面の両方を含める遠クリップ距離。

   // ビュー行列を直接保持するフラグと行列
   // trueの場合、Transform→行列変換を行わずviewMatrixOverrideを使用する
   bool hasViewMatrixOverride = false;               ///< TRSからの計算に代えて明示ビュー行列を使うかどうか。
   Matrix4x4 viewMatrixOverride = MakeIdentity4x4(); ///< 外部計算から直接供給されたビュー行列。

   CameraState() { transform.SetRotationQuaternion(Quaternion::Identity()); }

   /// @brief ビュー行列を直接セット（Transform経由の変換を行わない）
   void SetViewMatrix(const Matrix4x4& viewMatrix);

   /// @brief 有効なビュー行列を取得
   Matrix4x4 GetViewMatrix() const;

   /// @brief 2つのカメラ状態を線形補間
   static CameraState Lerp(const CameraState& a, const CameraState& b, float t);

   /// @brief ワールド行列を計算
   Matrix4x4 GetWorldMatrix() const;

   /// @brief 前方ベクトルを取得
   Vector3 GetForward() const;

   /// @brief 右ベクトルを取得
   Vector3 GetRight() const;

   /// @brief 上ベクトルを取得
   Vector3 GetUp() const;
};

} // namespace GameEngine
