#include "CameraGravityBridge.h"
#include "../Character/CharacterJump.h"
#include "../Vehicle/VehicleGroundMover.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"
#include "Utility/MathUtils/QuaternionOperations.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void CameraGravityBridge::Update(float /*deltaTime*/) {
   // オーナー不在時は更新しない
   if (!HasOwner()) { return; }

   // ワールド座標の取得元（Transform）を参照
   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   if (!transform) { return; }

   // 惑星中心→自身方向を正規化して重力Upを作る
   GameEngine::Vector3 toSelf = transform->transform.translation - planetCenter_;
   float len = toSelf.Length();
   if (len < 1e-4f) { return; }
   GameEngine::Vector3 gravityUp = toSelf * (1.0f / len);
   GameEngine::Vector3 pos       = transform->transform.translation;

   // GravityFollowCamera 側へ重力Upと注視対象を同期
   if (gravityFollowCamera_) {
      gravityFollowCamera_->SetGravityUp(gravityUp);
      gravityFollowCamera_->SetPivotTarget(pos);
   }

   // PlayerRearFollowCamera 側へ重力Up・注視対象・前方・空中状態を同期
   if (playerRearFollowCamera_) {
      GameEngine::Vector3 forward = { 0.0f, 0.0f, 1.0f };
      GameEngine::Quaternion rotation = transform->transform.GetActiveQuaternion();
      forward = GameEngine::RotateVector(forward, rotation);

      bool isAirborne = false;
      if (auto* jump = GetOwner().GetComponent<CharacterJump>()) {
         isAirborne = jump->IsJumping();
      }

      playerRearFollowCamera_->SetGravityUp(gravityUp);
      playerRearFollowCamera_->SetPivotTarget(pos);
      playerRearFollowCamera_->SetFollowForward(forward);
      playerRearFollowCamera_->SetAirborne(isAirborne);
   }

   // VehicleGroundMover から速度と autoSpeed を取得し、両カメラへ供給する
   float speed     = 0.0f;
   float autoSpeed = 13.0f;
   if (auto* mover = GetOwner().GetComponent<VehicleGroundMover>()) {
      speed     = mover->GetCurrentSpeed();
      autoSpeed = mover->autoSpeed;
   }
   if (gravityFollowCamera_) {
      gravityFollowCamera_->SetPlayerSpeed(speed);
      gravityFollowCamera_->SetAutoSpeed(autoSpeed);
   }
   if (playerRearFollowCamera_) {
      playerRearFollowCamera_->SetPlayerSpeed(speed);
      playerRearFollowCamera_->SetAutoSpeed(autoSpeed);
   }

   // PlanetLeashCamera 側へ重力Up・注視対象・惑星中心を同期
   if (planetLeashCamera_) {
      planetLeashCamera_->SetGravityUp(gravityUp);
      planetLeashCamera_->SetPivotTarget(pos);
      planetLeashCamera_->SetSphereCenter(planetCenter_);
   }
}

#ifdef USE_IMGUI
void CameraGravityBridge::DrawInspector() {
   if (!ImGui::CollapsingHeader("CameraGravityBridge")) {
      return;
   }
   ImGui::Separator();
   ImGui::Text("Planet Center: (%.2f, %.2f, %.2f)",
      planetCenter_.x, planetCenter_.y, planetCenter_.z);
   ImGui::Text("GravityFollowCamera: %s", gravityFollowCamera_ ? "Set" : "None");
   ImGui::Text("PlayerRearFollow:   %s", playerRearFollowCamera_ ? "Set" : "None");
   ImGui::Text("PlanetLeashCamera:   %s", planetLeashCamera_   ? "Set" : "None");
}
#endif

} // namespace App
