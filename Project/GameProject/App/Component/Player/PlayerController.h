#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector2.h"
#include "Utility/Math/Vector3.h"
#include "Scene/Camera/Camera.h"
#include "Scene/Camera/Components/OrbitalBody.h"
#include "../Camera/GravityFollowCamera.h"
#include "../Camera/PlanetLeashCamera.h"
#include "../Gravity/GravityBody.h"
#include "../Camera/ScreenSpaceBasis.h"
#include "../Character/CharacterWalker.h"
#include "../Character/CharacterJump.h"

namespace App {

/// @brief プレイヤー入力を収集し、CharacterWalker・CharacterJump へ委譲するコントローラー
class PlayerController final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "PlayerController";
   const char* GetTypeName() const override { return kTypeName; }

   void Update(float deltaTime) override;

   void SetCamera(GameEngine::Camera* camera) {
	  camera_ = camera;
	  if (basis_) { basis_->SetCamera(camera); }
   }
   void SetOrbitalBody(GameEngine::OrbitalBody* body) {
	  orbitalBody_ = body;
	  if (basis_) { basis_->SetOrbitalBody(body); }
   }
   void SetGravityFollowCamera(GravityFollowCamera* cam) {
	  gravityFollowCamera_ = cam;
	  if (basis_) { basis_->SetGravityFollowCamera(cam); }
   }
   void SetPlanetLeashCamera(PlanetLeashCamera* cam) {
	  planetLeashCamera_ = cam;
	  if (basis_) { basis_->SetPlanetLeashCamera(cam); }
   }

   GameEngine::Vector3 GetLastMoveDirection()    const { return walker_ ? walker_->GetLastMoveDirection() : GameEngine::Vector3{ 0.0f, 0.0f, 0.0f }; }
   GameEngine::Vector3 GetLastForwardProjected() const { return basis_ ? basis_->GetCachedForward() : GameEngine::Vector3{ 0.0f, 0.0f, 1.0f }; }
   GameEngine::Vector3 GetLastRightProjected()   const { return basis_ ? basis_->GetCachedRight() : GameEngine::Vector3{ 1.0f, 0.0f, 0.0f }; }

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   nlohmann::json Serialize() const override;
   void Deserialize(const nlohmann::json& data) override;

public:
   float inputDeadZone = 0.1f;

private:
   GameEngine::Vector2 CollectMoveInput() const;
   bool                CollectJumpInput() const;
   void                CacheComponents();

private:
   GameEngine::Camera* camera_ = nullptr;
   GameEngine::OrbitalBody* orbitalBody_ = nullptr;
   GravityFollowCamera* gravityFollowCamera_ = nullptr;
   PlanetLeashCamera* planetLeashCamera_ = nullptr;

   ScreenSpaceBasis* basis_ = nullptr;
   CharacterWalker* walker_ = nullptr;
   CharacterJump* jump_ = nullptr;
};

} // namespace App
