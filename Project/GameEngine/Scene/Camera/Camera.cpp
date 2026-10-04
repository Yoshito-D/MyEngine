#include "GameEngine/pch.h"
#include "GameEngine/Scene/Camera/Camera.h"
#include "GameEngine/Scene/Camera/Core/CameraState.h"
#include "GameEngine/Math/MathUtils.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"
#include <algorithm>
#include <cmath>

namespace GameEngine {
namespace {
// 全カメラが同じ GraphicsDevice を利用するため、カメラ生成時に毎回依存を渡さず共有する。
// 初期化順序は Framework が保証し、最初に登録されたデバイスをプロセス中維持する。
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;
constexpr float kMinFovY = 0.017453292f;  // 1度
constexpr float kMaxFovY = 3.12413936f;   // 179度

float ClampFovY(float fovY) {
	if (!std::isfinite(fovY)) {
		return Camera::kDefaultFovY;
	}
	// 0 度／180 度付近では透視投影の tan が退化するため、実用上安全な開区間へ収める。
	return std::clamp(fovY, kMinFovY, kMaxFovY);
}
}

void Camera::InitializeGraphicsDevice(GraphicsDevice* device) {
	if (sIsInitialized_) return;
	sDevice_ = device;
	sIsInitialized_ = true;
}

void Camera::Initialize(const Transform& transform, ProjectionType projectionType) {
	// 未指定項目はウィンドウ解像度とエンジン既定値から構成し、どちらの投影方式でも即座に使用可能にする。
	transform_ = transform;
	fovY_ = kDefaultFovY;
	aspectRatio_ = static_cast<float>(Window::kResolutionWidth) / static_cast<float>(Window::kResolutionHeight);
	nearClip_ = kDefaultNearClip;
	farClip_ = kDefaultFarClip;
	orthographicWidth_ = static_cast<float>(Window::kResolutionWidth);
	orthographicHeight_ = static_cast<float>(Window::kResolutionHeight);
	projectionType_ = projectionType;

	// カメラ位置は毎フレーム更新するため、アップロードバッファを永続マップしたまま保持する。
	cameraResource_ = ResourceHelper::CreateBufferResource(sDevice_->GetDevice(), sizeof(CameraForGPU));
	cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraForGpuData_));

	Update();
}

void Camera::SetFovY(float fovY) {
	fovY_ = ClampFovY(fovY);
   Update();
}

void Camera::Update() {
   // 投影の関連値を同じ境界で検証し、変更操作の終了時にキャッシュを一致させる。
   fovY_ = ClampFovY(fovY_);
   if (!std::isfinite(aspectRatio_) || aspectRatio_ <= 0.0f) aspectRatio_ = 1.0f;
   if (!std::isfinite(nearClip_) || nearClip_ <= 0.0f) nearClip_ = kDefaultNearClip;
   if (!std::isfinite(farClip_) || farClip_ <= nearClip_) farClip_ = nearClip_ + kDefaultFarClip;
   if (!std::isfinite(farClip_) || farClip_ <= nearClip_ || !std::isfinite(nearClip_ * farClip_)) {
      nearClip_ = kDefaultNearClip;
      farClip_ = kDefaultFarClip;
   }
   auto finiteOr = [](float value, float fallback) { return std::isfinite(value) ? value : fallback; };
   transform_.translation = { finiteOr(transform_.translation.x, 0.0f), finiteOr(transform_.translation.y, 0.0f), finiteOr(transform_.translation.z, 0.0f) };
   auto invertibleScale = [&](float value) { return std::abs(value) > 1.0e-6f ? finiteOr(value, 1.0f) : 1.0f; };
   transform_.scale = { invertibleScale(transform_.scale.x), invertibleScale(transform_.scale.y), invertibleScale(transform_.scale.z) };

	// TransformのrotationSourceに基づいてワールド行列を計算
	Matrix4x4 scaleMatrix = MakeScaleMatrix(transform_.scale);
	Matrix4x4 rotationMatrix = MakeRotateMatrix(transform_.GetActiveQuaternion());
	Matrix4x4 translateMatrix = MakeTranslateMatrix(transform_.translation);
	Matrix4x4 worldMatrix = scaleMatrix * rotationMatrix * translateMatrix;

	// カメラ自身のワールド変換の逆行列が、ワールドをカメラ空間へ移すビュー行列となる。
	Matrix4x4 viewMatrix = worldMatrix.Inverse();
	Matrix4x4 projectionMatrix = {};

	switch (projectionType_) {
		case ProjectionType::Perspective:
			projectionMatrix = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
			break;

		case ProjectionType::Orthographic:
			// UI 座標の上方向を正に保つため、上端・下端をこのエンジンの画面座標規約に合わせて渡す。
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

	// エンジンの行ベクトル規約に合わせて View * Projection の順に合成する。
	viewMatrix_ = viewMatrix;
	viewProjectionMatrix_ = viewMatrix * projectionMatrix;

	SetCameraForGpuData();
}

void Camera::ApplyState(const CameraState& state) {
   fovY_ = state.fov;
   nearClip_ = state.nearClip;
   farClip_ = state.farClip;
   if (state.hasViewMatrixOverride) {
      // 明示ビューでは従来どおり状態の位置をGPUへ渡し、回転・拡縮の解釈は変更しない。
      transform_.translation = state.transform.translation;
   } else {
      transform_ = state.transform;
   }
   Update();
   bool validOverride = state.hasViewMatrixOverride;
   for (const auto& row : state.viewMatrixOverride.m) for (float element : row) validOverride &= std::isfinite(element);
   if (validOverride) {
      viewMatrix_ = state.viewMatrixOverride;
      viewProjectionMatrix_ = viewMatrix_ * GetProjectionMatrix();
      SetCameraForGpuData();
   }
}

void Camera::SetOrthographicSize(float width, float height) {
	// 0 以下の寸法は射影行列の除算を成立させないため、現在の有効設定を維持する。
	if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0f || height <= 0.0f) {
		return;
	}

	orthographicWidth_ = width;
	orthographicHeight_ = height;
	aspectRatio_ = width / height;
	Update();
}

Matrix4x4 Camera::GetProjectionMatrix() const {
	// 射影方式やクリップ設定の現在値から再構築し、キャッシュされた ViewProjection からの逆算を避ける。
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
	// 行列は描画コマンド側で別途バインドし、この定数バッファにはライティング等が使う視点位置だけを置く。
	cameraForGpuData_->worldPosition = transform_.translation;
}

Vector3 Camera::GetForward() const {
	// 平行移動を含めず、現在選択中の Euler／Quaternion 回転源だけでローカル +Z 軸をワールド方向へ移す。
	Matrix4x4 rotationMatrix = MakeRotateMatrix(transform_.GetActiveQuaternion());
	Vector4 forward = TransformVectorByMatrix({ 0.0f, 0.0f, 1.0f, 1.0f }, rotationMatrix);
	return { forward.x, forward.y, forward.z };
}

} // namespace GameEngine
