#pragma once

#include "GameEngine/Math/VectorMath.h"
#include "GameEngine/Math/MathUtils.h"
#include <cmath>

namespace GameEngine {

/// @brief カメラの状態を表す値オブジェクト
struct CameraState {
   Transform transform;
   float fov = 0.45f;
   float nearClip = 0.01f;
   float farClip = 10000.0f;

   // ビュー行列を直接保持するフラグと行列
   // trueの場合、Transform→行列変換を行わずviewMatrixOverrideを使用する
   bool hasViewMatrixOverride = false;
   Matrix4x4 viewMatrixOverride = MakeIdentity4x4();

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
