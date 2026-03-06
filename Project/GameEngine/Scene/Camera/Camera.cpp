#include "pch.h"
#include "Camera.h"
#include "Utility/MathUtils.h"
#include "Core/Input/Input.h"
#include "ResourceHelper.h"

namespace GameEngine {
namespace {
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;
}

void Camera::InitializeGraphicsDevice(GraphicsDevice* device) {
   if (sIsInitialized_) return;
   sDevice_ = device;
   sIsInitialized_ = true;
}

void Camera::Initialize(const Transform& transform, ProjectionType projectionType) {
   transform_ = transform;
   fovY_ = 0.45f;
   aspectRatio_ = static_cast<float>(Window::kResolutionWidth) / static_cast<float>(Window::kResolutionHeight);
   nearClip_ = 0.01f;
   farClip_ = 100.0f;
   projectionType_ = projectionType;

   cameraResource_ = ResourceHelper::CreateBufferResource(sDevice_->GetDevice(), sizeof(CameraForGPU));
   // 書き込むためのアドレスを取得
   cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraForGpuData_));

   Update();
}

void Camera::Update() {
   Matrix4x4 worldMatrix;

   if (isUsingQuaternion_) {
	  // クォータニオンを使用する場合
	  Matrix4x4 scaleMatrix = MakeScaleMatrix(transform_.scale);
	  Matrix4x4 rotationMatrix = MakeRotateMatrix(quaternion_);
	  Matrix4x4 translateMatrix = MakeTranslateMatrix(transform_.translation);
	  worldMatrix = scaleMatrix * rotationMatrix * translateMatrix;
   } else {
	  // 通常のEuler角を使用する場合
	  worldMatrix = MakeAffineMatrix(transform_);
   }

   Matrix4x4 viewMatrix = worldMatrix.Inverse();
   Matrix4x4 projectionMatrix = {};

   switch (projectionType_) {
	  case ProjectionType::Perspective:
		 projectionMatrix = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
		 break;

	  case ProjectionType::Orthographic:
		 // 修正: top は正の値（画面上部）、bottom は負の値（画面下部）
		 projectionMatrix = MakeOrthographicMatrix(
			 static_cast<float>(-Window::kResolutionWidth) * 0.5f,   // left
			 static_cast<float>(Window::kResolutionHeight) * 0.5f,   // top（プラス）
			 static_cast<float>(Window::kResolutionWidth) * 0.5f,    // right
			 static_cast<float>(-Window::kResolutionHeight) * 0.5f,  // bottom（マイナス）
			 nearClip_, 
			 farClip_
		 );
		 break;
   }

   viewProjectionMatrix_ = viewMatrix * projectionMatrix;

   SetCameraForGpuData();
}

Matrix4x4 Camera::GetProjectionMatrix() const {
   switch (projectionType_) {
	  case ProjectionType::Perspective:
		 return MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
		 break;
	  case ProjectionType::Orthographic:
		 // 修正: top は正の値（画面上部）、bottom は負の値（画面下部）
		 return MakeOrthographicMatrix(
			 static_cast<float>(-Window::kResolutionWidth) * 0.5f,   // left
			 static_cast<float>(Window::kResolutionHeight) * 0.5f,   // top（プラス）
			 static_cast<float>(Window::kResolutionWidth) * 0.5f,    // right
			 static_cast<float>(-Window::kResolutionHeight) * 0.5f,  // bottom（マイナス）
			 0.0f, 
			 1000.0f
		 );
		 break;
	  default:
		 return MakeIdentity4x4();
		 break;
   }
}

void Camera::SetCameraForGpuData() {
   if (cameraForGpuData_ == nullptr)return;
   cameraForGpuData_->worldPosition = transform_.translation;
}

Vector3 Camera::GetForward() const {
   Matrix4x4 rotationMatrix;
   if (isUsingQuaternion_) {
	  rotationMatrix = MakeRotateMatrix(quaternion_);
   } else {
	  Matrix4x4 rotX = MakeRotateXMatrix(transform_.rotation.x);
	  Matrix4x4 rotY = MakeRotateYMatrix(transform_.rotation.y);
	  Matrix4x4 rotZ = MakeRotateZMatrix(transform_.rotation.z);
	  rotationMatrix = rotZ * rotY * rotX;
   }
   Vector4 forward = TransformVectorByMatrix({ 0.0f, 0.0f, 1.0f, 1.0f }, rotationMatrix);
   return { forward.x, forward.y, forward.z };
}

}