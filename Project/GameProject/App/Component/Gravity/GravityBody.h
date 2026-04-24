#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "Utility/Math/Quaternion.h"

namespace App {

/// @brief 重力を受けるオブジェクトのコンポーネント
/// 外部から与えられた重力方向（UpVector）に合わせて姿勢を回転させ、
/// 重力加速度を適用して速度・位置を更新する
class GravityBody final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "GravityBody";
   const char* GetTypeName() const override { return kTypeName; }

   void Update(float deltaTime) override;

   void SetTargetUpVector(const GameEngine::Vector3& targetUp);
   void SnapToUpVector(const GameEngine::Vector3& targetUp);
   void SetGravity(const GameEngine::Vector3& gravity);

   GameEngine::Vector3 GetVelocity() const { return velocity_; }
   void SetVelocity(const GameEngine::Vector3& velocity) { velocity_ = velocity; }
   GameEngine::Vector3 GetCurrentUpVector() const { return currentUpVector_; }

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   nlohmann::json Serialize() const override;
   void Deserialize(const nlohmann::json& data) override;

public:
   float rotationSpeed  = 5.0f;
   float gravityStrength = 9.8f;
   bool  useGravity     = true;

private:
   void UpdateRotation(float deltaTime);
   void UpdatePhysics(float deltaTime);

private:
   GameEngine::Vector3 currentUpVector_    = { 0.0f, 1.0f, 0.0f };
   GameEngine::Vector3 targetUpVector_     = { 0.0f, 1.0f, 0.0f };
   GameEngine::Vector3 gravityAcceleration_ = { 0.0f, 0.0f, 0.0f };
   GameEngine::Vector3 velocity_           = { 0.0f, 0.0f, 0.0f };
};

} // namespace App
