#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "Scene/Camera/Camera.h"
#include "Scene/Camera/Components/OrbitalBody.h"
#include "GravityFollowCamera.h"
#include "PlanetLeashCamera.h"

namespace App {

/// @brief カメラのスクリーン軸を重力平面へ投影し、移動基底（F_proj, R_proj）を提供するコンポーネント
class ScreenSpaceBasis final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "ScreenSpaceBasis";
   const char* GetTypeName() const override { return kTypeName; }

   void Update(float deltaTime) override { (void)deltaTime; }

   GameEngine::Vector3 GetForwardBasis(const GameEngine::Vector3& gravityUp) const;
   GameEngine::Vector3 GetRightBasis(const GameEngine::Vector3& gravityUp) const;
   void UpdateBasis(const GameEngine::Vector3& gravityUp);

   void SetCamera(GameEngine::Camera* camera)                     { camera_              = camera; }
   void SetOrbitalBody(GameEngine::OrbitalBody* body)             { orbitalBody_         = body; }
   void SetGravityFollowCamera(GravityFollowCamera* cam)          { gravityFollowCamera_ = cam; }
   void SetPlanetLeashCamera(PlanetLeashCamera* cam)              { planetLeashCamera_   = cam; }

   GameEngine::Vector3 GetCachedForward() const { return cachedForward_; }
   GameEngine::Vector3 GetCachedRight()   const { return cachedRight_; }

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   nlohmann::json Serialize() const override  { return {}; }
   void Deserialize(const nlohmann::json&) override {}

private:
   static GameEngine::Vector3 ProjectOnPlane(const GameEngine::Vector3& v, const GameEngine::Vector3& normal);

private:
   GameEngine::Camera*      camera_              = nullptr;
   GameEngine::OrbitalBody* orbitalBody_         = nullptr;
   GravityFollowCamera*     gravityFollowCamera_ = nullptr;
   PlanetLeashCamera*       planetLeashCamera_   = nullptr;

   mutable GameEngine::Vector3 cachedForward_ = { 0.0f, 0.0f, 1.0f };
   mutable GameEngine::Vector3 cachedRight_   = { 1.0f, 0.0f, 0.0f };
};

} // namespace App
