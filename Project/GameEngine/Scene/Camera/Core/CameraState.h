#pragma once
#include "Utility/VectorMath.h"
#include "Utility/MathUtils.h"
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

   CameraState() {
	  transform.SetRotationQuaternion(Quaternion::Identity());
   }

   /// @brief ビュー行列を直接セット（Transform経由の変換を行わない）
   void SetViewMatrix(const Matrix4x4& viewMatrix) {
	  viewMatrixOverride = viewMatrix;
	  hasViewMatrixOverride = true;
   }

   /// @brief 有効なビュー行列を取得
   Matrix4x4 GetViewMatrix() const {
	  if (hasViewMatrixOverride) {
		 return viewMatrixOverride;
	  }
	  return GetWorldMatrix().Inverse();
   }

   /// @brief 2つのカメラ状態を線形補間
   static CameraState Lerp(const CameraState& a, const CameraState& b, float t) {
	  CameraState result;

	  auto extractTransformFromView = [](const Matrix4x4& viewMatrix) {
		  Transform transform;
		  transform.scale = Vector3(1.0f, 1.0f, 1.0f);

		  // 位置はワールド行列（ビュー行列の逆行列）の平行移動成分から取得
		  Matrix4x4 worldMatrix = viewMatrix.Inverse();
		  transform.translation = { worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2] };

		  // MakeRotateMatrix はクォータニオンから「行に軸を格納」するrow-major形式で生成する。
		  // MatrixToQuaternion は「列に軸がある」標準数学規約を期待する。
		  // ビュー行列の回転部分はワールド回転行列の転置（= 列に軸がある形式）なので、
		  // ビュー行列の3x3部分を直接 MatrixToQuaternion に渡すことで正しいクォータニオンが得られる。
		  Matrix4x4 rotMatrix = MakeIdentity4x4();
		  for (int i = 0; i < 3; ++i)
			  for (int j = 0; j < 3; ++j)
				  rotMatrix.m[i][j] = viewMatrix.m[i][j];
		  transform.SetRotationQuaternion(MatrixToQuaternion(rotMatrix));

		  return transform;
	  };

	  Transform transformA = a.hasViewMatrixOverride ? extractTransformFromView(a.GetViewMatrix()) : a.transform;
	  Transform transformB = b.hasViewMatrixOverride ? extractTransformFromView(b.GetViewMatrix()) : b.transform;

	  result.transform.translation = Vector3::Lerp(transformA.translation, transformB.translation, t);
	  result.transform.scale = Vector3::Lerp(transformA.scale, transformB.scale, t);
	  result.transform.SetRotationQuaternion(
		 Quaternion::Slerp(transformA.GetActiveQuaternion(), transformB.GetActiveQuaternion(), t)
	  );
	  result.fov = a.fov + (b.fov - a.fov) * t;
	  result.nearClip = a.nearClip + (b.nearClip - a.nearClip) * t;
	  result.farClip = a.farClip + (b.farClip - a.farClip) * t;
	  return result;
   }

   /// @brief ワールド行列を計算
   Matrix4x4 GetWorldMatrix() const {
	  Matrix4x4 scaleMatrix = MakeScaleMatrix(transform.scale);
	  Matrix4x4 rotationMatrix = MakeRotateMatrix(transform.GetActiveQuaternion());
	  Matrix4x4 translateMatrix = MakeTranslateMatrix(transform.translation);
	  return scaleMatrix * rotationMatrix * translateMatrix;
   }

   /// @brief 前方ベクトルを取得
   Vector3 GetForward() const {
	  Matrix4x4 rotationMatrix = MakeRotateMatrix(transform.GetActiveQuaternion());
	  Vector4 forward = TransformVectorByMatrix({ 0.0f, 0.0f, 1.0f, 1.0f }, rotationMatrix);
	  return { forward.x, forward.y, forward.z };
   }

   /// @brief 右ベクトルを取得
   Vector3 GetRight() const {
	  Matrix4x4 rotationMatrix = MakeRotateMatrix(transform.GetActiveQuaternion());
	  Vector4 right = TransformVectorByMatrix({ 1.0f, 0.0f, 0.0f, 1.0f }, rotationMatrix);
	  return { right.x, right.y, right.z };
   }

   /// @brief 上ベクトルを取得
   Vector3 GetUp() const {
	  Matrix4x4 rotationMatrix = MakeRotateMatrix(transform.GetActiveQuaternion());
	  Vector4 up = TransformVectorByMatrix({ 0.0f, 1.0f, 0.0f, 1.0f }, rotationMatrix);
	  return { up.x, up.y, up.z };
   }
};

} // namespace GameEngine
