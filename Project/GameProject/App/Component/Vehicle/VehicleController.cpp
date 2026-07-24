#include "VehicleController.h"
#include "Object/Object.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

using namespace GameEngine;

namespace App {

void VehicleController::CacheComponents() {
   input_ = GetOwner().GetComponent<VehicleInputComponent>();
   mover_ = GetOwner().GetComponent<VehicleMover>();
   jump_  = GetOwner().GetComponent<CharacterJump>();
}

void VehicleController::Update(float deltaTime) {
   if (!HasOwner()) { return; }

   if (!input_ || !mover_ || !jump_) {
	  CacheComponents();
   }

   // 現在の重力Upを取得（無ければワールドUp）
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   Vector3 gravityUp = { 0.0f, 1.0f, 0.0f };
   if (gravityBody) { gravityUp = gravityBody->GetTargetUpVector(); }

   // 制御側は物理キーを知らず、意味付け済みの入力だけを消費する。
   if (gravityFollowCamera_ && input_) {
      const Vector2 camInput = input_->GetCameraLookInput();
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
   if (jump_ && input_ && input_->IsJumpTriggered()) {
	  jump_->Jump(gravityUp);
   }

   // 移動・姿勢入力を VehicleMover へ委譲
   if (mover_) {
	  const float steerInput = input_ ? input_->GetSteerInput() : 0.0f;
	  const float rollInput = input_ ? input_->GetRollInput() : 0.0f;
	  const float pitchInput = input_ ? input_->GetPitchInput() : 0.0f;
	  const bool driftInput = input_ && input_->IsDriftHeld();
	  mover_->ApplyMovement(steerInput, rollInput, pitchInput, driftInput, isGrounded, gravityUp, deltaTime);
   }
}

#ifdef USE_IMGUI
void VehicleController::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) { return; }
   ImGui::Separator();
   ImGui::TextUnformatted(Tr("入力は VehicleInputComponent から取得します", "Input is provided by VehicleInputComponent"));
}
#endif

nlohmann::json VehicleController::Serialize() const {
   return nlohmann::json::object();
}

void VehicleController::Deserialize(const nlohmann::json& data) {
   (void)data;
}

} // namespace App
