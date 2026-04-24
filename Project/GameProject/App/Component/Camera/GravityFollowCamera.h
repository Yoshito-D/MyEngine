#pragma once
#include "Scene/Camera/Core/ICinemachineComponent.h"
#include <algorithm>

namespace App {

/// @brief 重力追従型OrbitalCameraコンポーネント
class GravityFollowCamera : public GameEngine::ICinemachineComponent {
public:
   GravityFollowCamera()  = default;
   ~GravityFollowCamera() override = default;

   void MutateCameraState(GameEngine::CameraState& state, float deltaTime) override;
   GameEngine::CinemachineStage GetStage() const override { return GameEngine::CinemachineStage::Body; }

   void ProcessInput(const GameEngine::Vector2& mouseDelta, int32_t wheelDelta, bool isDragging);

   void SetGravityUp(const GameEngine::Vector3& up)     { gravityUp_    = up; }
   void SetPivotTarget(const GameEngine::Vector3& target) { pivotTarget_ = target; }

   GameEngine::Vector3 GetGravityUp()     const { return gravityUp_; }
   const GameEngine::Vector3& GetPivotTarget() const { return pivotTarget_; }
   float GetPitch()    const { return pitch_; }
   float GetDistance() const { return distance_; }
   void  SetDistance(float d) { distance_ = (std::max)(0.5f, d); }

   GameEngine::Vector3 GetCameraUp()    const;
   GameEngine::Vector3 GetCameraRight() const;

public:
   float rotateSpeed = 0.005f;
   float scrollSpeed = 1.0f / 120.0f;

private:
   float pitch_    = 1.0f;
   float distance_ = 10.0f;

   GameEngine::Vector3 flatForward_ = { 0.0f, 0.0f, 1.0f };
   GameEngine::Vector3 gravityUp_   = { 0.0f, 1.0f, 0.0f };
   GameEngine::Vector3 pivotTarget_ = { 0.0f, 0.0f, 0.0f };

   mutable GameEngine::Vector3 cachedRight_ = { 1.0f, 0.0f, 0.0f };
   mutable GameEngine::Vector3 cachedUp_    = { 0.0f, 1.0f, 0.0f };
};

} // namespace App
