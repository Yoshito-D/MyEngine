#include "PlayerController.h"
#include "Object/Object.h"
#include "Framework/EngineContext.h"
#include <cmath>
#include <algorithm>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

using namespace GameEngine;

namespace App {

void PlayerController::CacheComponents() {
   basis_  = GetOwner().GetComponent<ScreenSpaceBasis>();
   walker_ = GetOwner().GetComponent<CharacterWalker>();
   jump_   = GetOwner().GetComponent<CharacterJump>();

   if (basis_) {
      basis_->SetCamera(camera_);
      basis_->SetOrbitalBody(orbitalBody_);
      basis_->SetGravityFollowCamera(gravityFollowCamera_);
      basis_->SetPlanetLeashCamera(planetLeashCamera_);
   }
}

void PlayerController::Update(float deltaTime) {
   if (!HasOwner()) { return; }

   if (!basis_ || !walker_ || !jump_) {
      CacheComponents();
   }

   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   Vector3 gravityUp = { 0.0f, 1.0f, 0.0f };
   if (gravityBody) { gravityUp = gravityBody->GetCurrentUpVector(); }

   // 右スティック / 十字キーで GravityFollowCamera を回転
   if (gravityFollowCamera_) {
      Vector2 camInput = { 0.0f, 0.0f };

      // 右スティック
      Vector2 rStick = EngineContext::GetRightStick(0);
      if (std::abs(rStick.x) > inputDeadZone || std::abs(rStick.y) > inputDeadZone) {
         camInput = rStick;
      }

      // 十字キー（D-pad）
      if (EngineContext::IsKeyPressed(KeyCode::Right)) { camInput.x += 1.0f; }
      if (EngineContext::IsKeyPressed(KeyCode::Left))  { camInput.x -= 1.0f; }
      if (EngineContext::IsKeyPressed(KeyCode::Up))    { camInput.y -= 1.0f; }
      if (EngineContext::IsKeyPressed(KeyCode::Down))  { camInput.y += 1.0f; }

      if (camInput.x != 0.0f || camInput.y != 0.0f) {
         constexpr float kCamRotateScale = 150.0f;
         Vector2 delta = { camInput.x * kCamRotateScale * deltaTime,
                           camInput.y * kCamRotateScale * deltaTime };
         gravityFollowCamera_->ProcessInput(delta, 0, true);
      }
   }

   if (jump_ && CollectJumpInput()) {
      jump_->Jump(gravityUp);
   }

   if (walker_) {
      bool isGrounded = !(jump_ && jump_->IsJumping());
      Vector2 input = CollectMoveInput();
      float inputLength = std::sqrt(input.x * input.x + input.y * input.y);
      if (inputLength >= inputDeadZone) {
         float normalizedLength = std::min(inputLength, 1.0f);
         Vector2 normalizedInput = {
            (input.x / inputLength) * normalizedLength,
            (input.y / inputLength) * normalizedLength
         };
         walker_->ApplyMovement(normalizedInput, gravityUp, deltaTime, isGrounded);
      } else {
         walker_->ApplyMovement({ 0.0f, 0.0f }, gravityUp, deltaTime, isGrounded);
      }
   }
}

Vector2 PlayerController::CollectMoveInput() const {
   Vector2 input = { 0.0f, 0.0f };
   if (EngineContext::IsKeyPressed(KeyCode::W)) { input.y += 1.0f; }
   if (EngineContext::IsKeyPressed(KeyCode::S)) { input.y -= 1.0f; }
   if (EngineContext::IsKeyPressed(KeyCode::A)) { input.x -= 1.0f; }
   if (EngineContext::IsKeyPressed(KeyCode::D)) { input.x += 1.0f; }

   Vector2 stick = EngineContext::GetLeftStick(0);
   if (std::abs(stick.x) > inputDeadZone || std::abs(stick.y) > inputDeadZone) {
      input = stick;
   }
   return input;
}

bool PlayerController::CollectJumpInput() const {
   return EngineContext::IsKeyTriggered(KeyCode::Space);
}

#ifdef USE_IMGUI
void PlayerController::DrawInspector() {
   if (!ImGui::CollapsingHeader("PlayerController", ImGuiTreeNodeFlags_DefaultOpen)) {
      return;
   }
   ImGui::Separator();
   ImGui::DragFloat("Input DeadZone", &inputDeadZone, 0.01f, 0.0f, 1.0f);
}
#endif

nlohmann::json PlayerController::Serialize() const {
   nlohmann::json json;
   json["inputDeadZone"] = inputDeadZone;
   return json;
}

void PlayerController::Deserialize(const nlohmann::json& data) {
   if (data.contains("inputDeadZone")) { inputDeadZone = data["inputDeadZone"]; }
}

} // namespace App
