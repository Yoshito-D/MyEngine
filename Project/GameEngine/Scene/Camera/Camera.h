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
	/// @brief シェーダーが参照するカメラ定数
	struct CameraForGPU {
		Vector3 worldPosition; ///< カメラのワールド位置
	};

	/// @brief プロジェクションタイプ
	enum class ProjectionType {
		Perspective, ///< 透視投影
		Orthographic ///< 平行投影
	};

	/// @brief 全CameraがGPU定数バッファ生成に使用するデバイスを設定する
	/// @param device 初期化済みGraphicsDevice
	static void InitializeGraphicsDevice(GraphicsDevice* device);

	/// @brief 初期化
	/// @param transform カメラのトランスフォーム
	/// @param projectionType プロジェクションタイプ
	void Initialize(const Transform& transform = Transform(), ProjectionType projectionType = ProjectionType::Perspective);

	/// @brief 更新（ビュープロジェクション行列を再計算）
	void Update();

	/// @brief カメラの位置・回転・拡縮をまとめて設定する
	/// @param transform 新しいトランスフォーム
	void SetTransform(const Transform& transform) { transform_ = transform; }
	/// @brief カメラのトランスフォームを取得する
	/// @return 現在のトランスフォーム
	const Transform& GetTransform() const { return transform_; }

	/// @brief カメラのワールド位置を設定する
	/// @param position ワールド位置
	void SetPosition(const Vector3& position) { transform_.translation = position; }
	/// @brief カメラのワールド位置を取得する
	/// @return ワールド位置
	Vector3 GetPosition() const { return transform_.translation; }

	/// @brief カメラ回転をEuler角で設定する
	/// @param rotation XYZ回転（ラジアン）
	void SetRotation(const Vector3& rotation) { transform_.SetRotationEuler(rotation); }
	/// @brief カメラ回転をEuler角で取得する
	/// @return XYZ回転（ラジアン）
	Vector3 GetRotation() const { return transform_.GetActiveEuler(); }

	/// @brief カメラ回転をQuaternionで設定する
	/// @param quaternion ワールド回転
	void SetQuaternion(const Quaternion& quaternion) { transform_.SetRotationQuaternion(quaternion); }
	/// @brief カメラ回転をQuaternionで取得する
	/// @return ワールド回転
	Quaternion GetQuaternion() const { return transform_.GetActiveQuaternion(); }

	/// @brief カメラのスケールを設定する
	/// @param scale 各軸のスケール
	void SetScale(const Vector3& scale) { transform_.scale = scale; }
	/// @brief カメラのスケールを取得する
	/// @return 各軸のスケール
	Vector3 GetScale() const { return transform_.scale; }

	// 投影設定
	/// @brief 透視投影の垂直 FOV を設定する
	/// @param fovY 垂直視野角（ラジアン）
	/// @details 無効値や負値で射影行列が壊れないよう、安全な角度範囲へ丸める。
	void SetFovY(float fovY);
	/// @brief 透視投影の垂直視野角を取得する
	/// @return 垂直視野角（ラジアン）
	float GetFovY() const { return fovY_; }

	/// @brief 投影面のアスペクト比を設定する
	/// @param aspectRatio 幅を高さで割った比率
	void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }
	/// @brief 投影面のアスペクト比を取得する
	/// @return 幅を高さで割った比率
	float GetAspectRatio() const { return aspectRatio_; }

	/// @brief ニアクリップ距離を設定する
	/// @param nearClip カメラから近平面までの距離
	void SetNearClip(float nearClip) { nearClip_ = nearClip; }
	/// @brief ニアクリップ距離を取得する
	/// @return カメラから近平面までの距離
	float GetNearClip() const { return nearClip_; }

	/// @brief ファークリップ距離を設定する
	/// @param farClip カメラから遠平面までの距離
	void SetFarClip(float farClip) { farClip_ = farClip; }
	/// @brief ファークリップ距離を取得する
	/// @return カメラから遠平面までの距離
	float GetFarClip() const { return farClip_; }

	/// @brief 透視投影または平行投影へ切り替える
	/// @param type 使用する投影方式
	void SetProjectionType(ProjectionType type) { projectionType_ = type; }
	/// @brief 現在の投影方式を取得する
	/// @return 透視投影または平行投影
	ProjectionType GetProjectionType() const { return projectionType_; }

	/// @brief 平行投影の投影サイズを設定する
	/// @param width 投影幅
	/// @param height 投影高さ
	void SetOrthographicSize(float width, float height);

	/// @brief 現在のビュー・プロジェクション合成行列を取得する
	/// @return ビュープロジェクション行列
	Matrix4x4 GetViewProjectionMatrix() const { return viewProjectionMatrix_; }
	/// @brief 外部計算したビュー・プロジェクション行列で上書きする
	/// @param matrix 設定する合成行列
	void SetViewProjectionMatrix(const Matrix4x4& matrix) { viewProjectionMatrix_ = matrix; }
	/// @brief 現在のビュー行列を取得する
	/// @return ワールドからカメラ空間への行列
	Matrix4x4 GetViewMatrix() const { return viewMatrix_; }
	/// @brief ImGuizmoへ渡す変更可能なCameraのビュー行列値を取得する
	/// @return ワールドからカメラ空間への行列
	Matrix4x4 GetViewMatrixForImGuizmo() { return viewMatrix_; }
	/// @brief 外部計算したビュー行列で上書きする
	/// @param matrix ワールドからカメラ空間への行列
	void SetViewMatrix(const Matrix4x4& matrix) { viewMatrix_ = matrix; }
	/// @brief 現在の設定から投影行列を構築する
	/// @return 投影方式に対応するプロジェクション行列
	Matrix4x4 GetProjectionMatrix() const;

	/// @brief カメラのワールド前方ベクトルを取得する
	/// @return 正規化された前方方向
	Vector3 GetForward() const;

	/// @brief カメラ定数を保持するGPUリソースを取得する
	/// @return CameraForGPU用定数バッファ
	ID3D12Resource* GetCameraResource() const { return cameraResource_.Get(); }
	/// @brief 現在のワールド位置をGPU定数バッファへ反映する
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
