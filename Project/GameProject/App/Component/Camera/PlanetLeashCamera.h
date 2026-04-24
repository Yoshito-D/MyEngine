#pragma once
#include "Scene/Camera/Core/ICinemachineComponent.h"
#include <algorithm>

namespace App {

/// @brief 惑星対応レアッシュカメラコンポーネント
class PlanetLeashCamera : public GameEngine::ICinemachineComponent {
public:
   PlanetLeashCamera()  = default;
   ~PlanetLeashCamera() override = default;

   void MutateCameraState(GameEngine::CameraState& state, float deltaTime) override;
   GameEngine::CinemachineStage GetStage() const override { return GameEngine::CinemachineStage::Body; }
   const char* GetComponentName() const override { return "PlanetLeashCamera"; }

   void SetPivotTarget(const GameEngine::Vector3& target) { pivotTarget_ = target; }
   void SetSphereCenter(const GameEngine::Vector3& center) { sphereCenter_ = center; }
   void SetInitialEyePosition(const GameEngine::Vector3& eye) { eyePos_ = eye; isInitialized_ = true; }
   void SetGravityUp(const GameEngine::Vector3& up) { gravityUp_ = up; }

   GameEngine::Vector3 GetCameraUp()    const { return cachedUp_; }
   GameEngine::Vector3 GetCameraRight() const { return cachedRight_; }

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

public:
   float maxFollowDistance  = 8.0f;
   float followSpeed        = 6.0f;
   float minPlanetDistance  = 7.0f;
   bool  useGravityUp       = true;

private:
   GameEngine::Vector3 pivotTarget_  = { 0.0f, 0.0f, 0.0f };
   GameEngine::Vector3 sphereCenter_ = { 0.0f, 0.0f, 0.0f };
   GameEngine::Vector3 gravityUp_    = { 0.0f, 1.0f, 0.0f };

   GameEngine::Vector3 eyePos_        = { 0.0f, 5.0f, -15.0f };
   bool                isInitialized_ = false;
   GameEngine::Vector3 prevGravityUp_ = { 0.0f, 1.0f, 0.0f };
   GameEngine::Vector3 eyeRelUp_      = { 0.0f, 1.0f, 0.0f };

   mutable GameEngine::Vector3 cachedRight_ = { 1.0f, 0.0f, 0.0f };
   mutable GameEngine::Vector3 cachedUp_    = { 0.0f, 1.0f, 0.0f };
};

} // namespace App
