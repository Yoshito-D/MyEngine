#pragma once
#include "Utility/VectorMath.h"
#include "Core/Window/Window.h"
#include "GraphicsDevice.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

namespace GameEngine {
class Input;

/// @brief カメラクラス
class Camera {
public:
   struct CameraForGPU {
	  Vector3 worldPosition;
   };

   static void InitializeGraphicsDevice(GraphicsDevice* device);

public:
   /// @brief カメラのプロジェクションタイプ
   enum class ProjectionType {
	  Perspective, // 透視投影
	  Orthographic // 平行投影
   };

   /// @brief カメラの初期化
   /// @param transform カメラのトランスフォーム
   /// @param projectionType プロジェクションタイプ
   void Initialize(const Transform& transform = Transform(), ProjectionType projectionType = ProjectionType::Perspective);

   /// @brief カメラの更新
   void Update();

   void SetTransform(const Transform& transform) { transform_ = transform; }

   void SetPosition(const Vector3& translation) { transform_.translation = translation; }
   void SetRotation(const Vector3& rotation) { transform_.rotation = rotation; }
   void SetScale(const Vector3& scale) { transform_.scale = scale; }

   void SetFovY(float fovY) { fovY_ = fovY; }

   void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }

   void SetNearClip(float nearClip) { nearClip_ = nearClip; }

   void SetFarClip(float farClip) { farClip_ = farClip; }

   void SetProjectionType(ProjectionType projectionType) { projectionType_ = projectionType; }

   void SetViewProjectionMatrix(Matrix4x4 viewProjectionMatrix) { viewProjectionMatrix_ = viewProjectionMatrix; }

   Matrix4x4 GetViewProjectionMatrix() const { return viewProjectionMatrix_; }

   Matrix4x4 GetProjectionMatrix() const;

   Transform GetTransform() const { return transform_; }

   Vector3 GetPosition() const { return transform_.translation; }
   Vector3 GetRotation() const { return transform_.rotation; }
   Vector3 GetScale() const { return transform_.scale; }

   void SetUsingQuaternion(bool isUsing) { isUsingQuaternion_ = isUsing; }

   bool IsUsingQuaternion() const { return isUsingQuaternion_; }

   Quaternion GetRotationQuaternion() const { return quaternion_; }

   void SetQuaternion(const Quaternion& quaternion) {
	  quaternion_ = quaternion;
   }

   Vector3 GetForward() const;

   ID3D12Resource* GetCameraResource() const { return cameraResource_.Get(); }

   void SetCameraForGpuData();
private:
   float fovY_ = 0.0f;
   float aspectRatio_ = 0.0f;
   float nearClip_ = 0.0f;
   float farClip_ = 0.0f;
   bool isUsingQuaternion_ = false;
   ProjectionType projectionType_ = ProjectionType::Perspective;
   Transform transform_{};
   Quaternion quaternion_ = Quaternion::Identity();
   Matrix4x4 viewProjectionMatrix_{};

   Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_ = nullptr;
   CameraForGPU* cameraForGpuData_ = nullptr;
};
}