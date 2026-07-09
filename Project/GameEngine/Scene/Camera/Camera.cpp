#include "pch.h"
#include "Camera.h"
#include "Utility/MathUtils.h"
#include "ResourceHelper.h"
#include <algorithm>
#include <cmath>

namespace GameEngine {
namespace {
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;
constexpr float kDefaultFovY = 0.45f;
constexpr float kMinFovY = 0.017453292f;  // 1 degree
constexpr float kMaxFovY = 3.12413936f;   // 179 degrees

float ClampFovY(float fovY) {
	if (!std::isfinite(fovY)) {
		return kDefaultFovY;
	}
	return std::clamp(fovY, kMinFovY, kMaxFovY);
}
}

void Camera::InitializeGraphicsDevice(GraphicsDevice* device) {
	if (sIsInitialized_) return;
	sDevice_ = device;
	sIsInitialized_ = true;
}

void Camera::Initialize(const Transform& transform, ProjectionType projectionType) {
	transform_ = transform;
	fovY_ = kDefaultFovY;
	aspectRatio_ = static_cast<float>(Window::kResolutionWidth) / static_cast<float>(Window::kResolutionHeight);
	nearClip_ = 0.01f;
	farClip_ = 100.0f;
	orthographicWidth_ = static_cast<float>(Window::kResolutionWidth);
	orthographicHeight_ = static_cast<float>(Window::kResolutionHeight);
	projectionType_ = projectionType;

	cameraResource_ = ResourceHelper::CreateBufferResource(sDevice_->GetDevice(), sizeof(CameraForGPU));
	cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraForGpuData_));

	Update();
}

void Camera::SetFovY(float fovY) {
	fovY_ = ClampFovY(fovY);
}

void Camera::Update() {
	// TransformのrotationSourceに基づいてワールド行列を計算
	Matrix4x4 scaleMatrix = MakeScaleMatrix(transform_.scale);
	Matrix4x4 rotationMatrix = MakeRotateMatrix(transform_.GetActiveQuaternion());
	Matrix4x4 translateMatrix = MakeTranslateMatrix(transform_.translation);
	Matrix4x4 worldMatrix = scaleMatrix * rotationMatrix * translateMatrix;

	Matrix4x4 viewMatrix = worldMatrix.Inverse();
	Matrix4x4 projectionMatrix = {};

	switch (projectionType_) {
		case ProjectionType::Perspective:
			projectionMatrix = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
			break;

		case ProjectionType::Orthographic:
			projectionMatrix = MakeOrthographicMatrix(
				-orthographicWidth_ * 0.5f,
				orthographicHeight_ * 0.5f,
				orthographicWidth_ * 0.5f,
				-orthographicHeight_ * 0.5f,
				nearClip_,
				farClip_
			);
			break;
	}

	viewMatrix_ = viewMatrix;
	viewProjectionMatrix_ = viewMatrix * projectionMatrix;

	SetCameraForGpuData();
}

void Camera::SetOrthographicSize(float width, float height) {
	if (width <= 0.0f || height <= 0.0f) {
		return;
	}

	orthographicWidth_ = width;
	orthographicHeight_ = height;
	aspectRatio_ = width / height;
	Update();
}

Matrix4x4 Camera::GetProjectionMatrix() const {
	switch (projectionType_) {
		case ProjectionType::Perspective:
			return MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
		case ProjectionType::Orthographic:
			return MakeOrthographicMatrix(
				-orthographicWidth_ * 0.5f,
				orthographicHeight_ * 0.5f,
				orthographicWidth_ * 0.5f,
				-orthographicHeight_ * 0.5f,
				nearClip_,
				farClip_
			);
		default:
			return MakeIdentity4x4();
	}
}

void Camera::SetCameraForGpuData() {
	if (cameraForGpuData_ == nullptr) return;
	cameraForGpuData_->worldPosition = transform_.translation;
}

Vector3 Camera::GetForward() const {
	Matrix4x4 rotationMatrix = MakeRotateMatrix(transform_.GetActiveQuaternion());
	Vector4 forward = TransformVectorByMatrix({ 0.0f, 0.0f, 1.0f, 1.0f }, rotationMatrix);
	return { forward.x, forward.y, forward.z };
}

} // namespace GameEngine
