#pragma once
#include "Camera.h"
#include "Input/Input.h"
#include "Utility/MathUtils.h"

namespace GameEngine {
class DebugCamera {
public:
   /// @brief デバッグカメラの更新
   void Update();

   /// @brief カメラを設定する
   /// @param camera カメラ
   void SetCamera(Camera* camera) { camera_ = camera; }

   /// @brief ピボットターゲットとの距離を設定する
   void SetDistance(float distance);

   void ApplyCameraTransform();
private:
   Camera* camera_ = nullptr;

   float yaw_ = DirectX::XM_PI;
   float pitch_ = ToRadians(-45.0f);
   float distance_ = 25.0f;
   Vector3 pivotTarget_ = { 0, 0, 0 };

   Matrix4x4 rotationMatrix_ = MakeIdentity4x4();
};
}