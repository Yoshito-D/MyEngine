#include "VehicleController.h"
#include "Object/Object.h"
#include "Framework/EngineContext.h"
#include <cmath>
#include <algorithm>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

using namespace GameEngine;

namespace App {

void VehicleController::CacheComponents() {
   mover_ = GetOwner().GetComponent<VehicleMover>();
   jump_  = GetOwner().GetComponent<CharacterJump>();
}

void VehicleController::Update(float deltaTime) {
   if (!HasOwner()) { return; }

   if (!mover_ || !jump_) {
	  CacheComponents();
   }

   // 現在の重力Upを取得（無ければワールドUp）
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   Vector3 gravityUp = { 0.0f, 1.0f, 0.0f };
   if (gravityBody) { gravityUp = gravityBody->GetTargetUpVector(); }

   // カメラ入力を GravityFollowCamera へ反映（矢印キー / 右スティック）
   if (gravityFollowCamera_) {
      Vector2 camInput = { 0.0f, 0.0f };

      Vector2 rStick = EngineContext::GetRightStick(0);
      if (std::abs(rStick.x) > inputDeadZone || std::abs(rStick.y) > inputDeadZone) {
         camInput = rStick;
      }

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

   // 接地判定（ジャンプ中でなければ接地扱い）
   bool isGrounded = !(jump_ && jump_->IsJumping());

   // ジャンプ入力
   if (jump_ && CollectJumpInput()) {
	  jump_->Jump(gravityUp);
   }

   // 移動・姿勢入力を VehicleMover へ委譲
   if (mover_) {
	  float steerInput = CollectSteerInput();
	  float pitchInput = CollectPitchInput();
	  bool  driftInput = CollectDriftInput();
	  mover_->ApplyMovement(steerInput, pitchInput, driftInput, isGrounded, gravityUp, deltaTime);
   }
}

float VehicleController::CollectSteerInput() const {
   float input = 0.0f;
   if (EngineContext::IsKeyPressed(KeyCode::A)) { input -= 1.0f; }
   if (EngineContext::IsKeyPressed(KeyCode::D)) { input += 1.0f; }

   // 左スティック X が有効なら優先採用
   Vector2 stick = EngineContext::GetLeftStick(0);
   if (std::abs(stick.x) > inputDeadZone) { input = stick.x; }

   return std::clamp(input, -1.0f, 1.0f);
}

float VehicleController::CollectPitchInput() const {
   float input = 0.0f;
   if (EngineContext::IsKeyPressed(KeyCode::W)) { input += 1.0f; }
   if (EngineContext::IsKeyPressed(KeyCode::S)) { input -= 1.0f; }

   // 左スティック Y が有効なら優先採用
   Vector2 stick = EngineContext::GetLeftStick(0);
   if (std::abs(stick.y) > inputDeadZone) { input = stick.y; }

   return std::clamp(input, -1.0f, 1.0f);
}

bool VehicleController::CollectJumpInput() const {
   return EngineContext::IsKeyTriggered(KeyCode::Space);
}

bool VehicleController::CollectDriftInput() const {
   // Q キー押し続けでドリフト入力とする。
   // IsKeyPressed は押し続けている間 true を返すため、
   // ButtonMode のドリフトは「押している間だけ維持」という自然な操作感になる。
   if (EngineContext::IsKeyPressed(KeyCode::Q)) { return true; }

   // ゲームパッドの LB ボタン（LeftShoulder）が押されている場合もドリフト入力とする。
   if (EngineContext::IsGamePadButtonPressed(GamePadButton::LeftShoulder, 0)) { return true; }

   return false;
}

#ifdef USE_IMGUI
void VehicleController::DrawInspector() {
   if (!ImGui::CollapsingHeader("VehicleController")) { return; }
   ImGui::Separator();
   ImGui::DragFloat("Input DeadZone", &inputDeadZone, 0.01f, 0.0f, 1.0f);
}
#endif

nlohmann::json VehicleController::Serialize() const {
   nlohmann::json json;
   json["inputDeadZone"] = inputDeadZone;
   return json;
}

void VehicleController::Deserialize(const nlohmann::json& data) {
   if (data.contains("inputDeadZone")) { inputDeadZone = data["inputDeadZone"]; }
}

} // namespace App
