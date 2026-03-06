#include "pch.h"
#include "DebugCamera.h"
#include <EngineContext.h>

namespace GameEngine {
void DebugCamera::Update() {

   Vector2 mouseDelta = EngineContext::GetMouseDelta();
   int32_t wheel = EngineContext::GetMouseWheelDelta();

   constexpr float kRotateSpeed = 0.01f;
   constexpr float kScrollSpeed = 1.0f / 120.0f;

   constexpr float kBaseMoveSpeed = 0.0008f;
   float moveSpeed = kBaseMoveSpeed * distance_;

   if (EngineContext::IsMousePressed(2)) {
	  // シフトが押されている時は、平行移動。押されていないときは、ピボッド回転
	  if (EngineContext::IsKeyPressed(DIK_LSHIFT)) {
		 Vector3 right = TransformNormal({ 1, 0, 0 }, rotationMatrix_);
		 Vector3 up = TransformNormal({ 0, 1, 0 }, rotationMatrix_);
		 Vector3 move{};
		 move += right * (mouseDelta.x * moveSpeed);
		 move += up * (mouseDelta.y * moveSpeed);

		 pivotTarget_ += move;
	  } else {
		 yaw_ += mouseDelta.x * kRotateSpeed;
		 pitch_ -= mouseDelta.y * kRotateSpeed;
	  }
   }

   // ホイールで前後移動
   if (wheel != 0) {
	  distance_ -= wheel * kScrollSpeed;
	  distance_ = std::max(0.5f, distance_);
   }

   rotationMatrix_ = MakeRotateXMatrix(pitch_) * MakeRotateYMatrix(yaw_);

   Vector3 offset = TransformCoordinate({ 0, 0, distance_ }, rotationMatrix_);
   Vector3 eye = pivotTarget_ + offset;

   Vector3 up = TransformNormal({ 0, 1, 0 }, rotationMatrix_);

   Matrix4x4 viewMatrix = MakeLookAtMatrix(eye, pivotTarget_, up);
   Matrix4x4 projectionMatrix = camera_->GetProjectionMatrix();
   Matrix4x4 viewProjectionMatrix = viewMatrix * projectionMatrix;

   camera_->SetViewProjectionMatrix(viewProjectionMatrix);
}

void DebugCamera::SetDistance(float distance) {
   distance_ = std::max(0.1f, distance);
}

void DebugCamera::ApplyCameraTransform() {
   if (camera_ != nullptr) {
	  Vector3 offset = TransformCoordinate({ 0, 0, distance_ }, rotationMatrix_);
	  Vector3 eye = pivotTarget_ + offset;

	  Vector3 up = TransformNormal({ 0, 1, 0 }, rotationMatrix_);
	  Matrix4x4 viewMatrix = MakeLookAtMatrix(eye, pivotTarget_, up);
	  Matrix4x4 worldMatrix = viewMatrix.Inverse();

	  Transform transform;
	  transform.scale = { 1,1,1 };
	  transform.translation = eye;

	  if (camera_->IsUsingQuaternion()) {
		 // クォータニオンを使用する場合は回転行列からクォータニオンを抽出
		 Matrix4x4 rotationOnly = worldMatrix;
		 rotationOnly.m[3][0] = 0.0f;
		 rotationOnly.m[3][1] = 0.0f;
		 rotationOnly.m[3][2] = 0.0f;
		 rotationOnly.m[0][3] = 0.0f;
		 rotationOnly.m[1][3] = 0.0f;
		 rotationOnly.m[2][3] = 0.0f;
		 rotationOnly.m[3][3] = 1.0f;
		 
		 Quaternion quaternion = MatrixToQuaternion(rotationOnly);
		 camera_->SetQuaternion(quaternion);
		 transform.rotation = { 0, 0, 0 }; // クォータニオン使用時はEuler角は使用しない
	  } else {
		 // Euler角を使用する場合
		 Vector3 euler = ExtractYawPitchRoll(worldMatrix);
		 transform.rotation = euler;
	  }

	  camera_->SetTransform(transform);
	  camera_->SetCameraForGpuData();
   }
}
}
