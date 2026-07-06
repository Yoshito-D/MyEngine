#pragma once
#include "Utility/VectorMath.h"
#include "Core/Window/Window.h"
#include "GraphicsDevice.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

namespace GameEngine {

/// @brief カメラクラス（GPUリソース管理とマトリックス計算）
class Camera {
public:
	struct CameraForGPU {
		Vector3 worldPosition;
	};

	/// @brief プロジェクションタイプ
	enum class ProjectionType {
		Perspective,  // 透視投影
		Orthographic  // 平行投影
	};

	static void InitializeGraphicsDevice(GraphicsDevice* device);

	/// @brief 初期化
	/// @param transform カメラのトランスフォーム
	/// @param projectionType プロジェクションタイプ
	void Initialize(const Transform& transform = Transform(), ProjectionType projectionType = ProjectionType::Perspective);

	/// @brief 更新（ビュープロジェクション行列を再計算）
	void Update();

	// Transform設定
	void SetTransform(const Transform& transform) { transform_ = transform; }
	const Transform& GetTransform() const { return transform_; }

	void SetPosition(const Vector3& position) { transform_.translation = position; }
	Vector3 GetPosition() const { return transform_.translation; }

	void SetRotation(const Vector3& rotation) { transform_.SetRotationEuler(rotation); }
	Vector3 GetRotation() const { return transform_.GetActiveEuler(); }

	void SetQuaternion(const Quaternion& quaternion) { transform_.SetRotationQuaternion(quaternion); }
	Quaternion GetQuaternion() const { return transform_.GetActiveQuaternion(); }

	void SetScale(const Vector3& scale) { transform_.scale = scale; }
	Vector3 GetScale() const { return transform_.scale; }

	// 投影設定
	void SetFovY(float fovY) { fovY_ = fovY; }
	float GetFovY() const { return fovY_; }

	void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }
	float GetAspectRatio() const { return aspectRatio_; }

	void SetNearClip(float nearClip) { nearClip_ = nearClip; }
	float GetNearClip() const { return nearClip_; }

	void SetFarClip(float farClip) { farClip_ = farClip; }
	float GetFarClip() const { return farClip_; }

	void SetProjectionType(ProjectionType type) { projectionType_ = type; }
	ProjectionType GetProjectionType() const { return projectionType_; }

	/// @brief 平行投影の投影サイズを設定する
	/// @param width 投影幅
	/// @param height 投影高さ
	void SetOrthographicSize(float width, float height);

	// 行列アクセス
	Matrix4x4 GetViewProjectionMatrix() const { return viewProjectionMatrix_; }
	void SetViewProjectionMatrix(const Matrix4x4& matrix) { viewProjectionMatrix_ = matrix; }
	Matrix4x4 GetViewMatrix() const { return viewMatrix_; }
	Matrix4x4 GetViewMatrixForImGuizmo() { return viewMatrix_; }
	void SetViewMatrix(const Matrix4x4& matrix) { viewMatrix_ = matrix; }
	Matrix4x4 GetProjectionMatrix() const;

	// 方向ベクトル
	Vector3 GetForward() const;

	// GPUリソース
	ID3D12Resource* GetCameraResource() const { return cameraResource_.Get(); }
	void SetCameraForGpuData();

private:
	Transform transform_;
	float fovY_ = 0.45f;
	float aspectRatio_ = 0.0f;
	float nearClip_ = 0.01f;
	float farClip_ = 100.0f;
	float orthographicWidth_ = static_cast<float>(Window::kResolutionWidth);
	float orthographicHeight_ = static_cast<float>(Window::kResolutionHeight);
	ProjectionType projectionType_ = ProjectionType::Perspective;

	Matrix4x4 viewProjectionMatrix_{};
	Matrix4x4 viewMatrix_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_ = nullptr;
	CameraForGPU* cameraForGpuData_ = nullptr;
};

} // namespace GameEngine
