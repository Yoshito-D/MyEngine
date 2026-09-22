#include "VehicleController.h"
#include "Object/Object.h"
#include "Scene/SceneWorld.h"
#include "Scene/Camera/Core/VirtualCamera.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

using namespace GameEngine;

namespace App {

void VehicleController::SetGravityFollowCamera(GravityFollowCamera* camera) {
   const auto* ownerCamera = camera ? camera->GetOwnerCamera() : nullptr;
   gravityFollowCameraId_ = ownerCamera ? ownerCamera->GetId() : std::string{};
}

Vector3 VehicleController::GetLastMoveDirection() const {
   const auto* mover = HasOwner() ? GetOwner().GetComponent<VehicleMover>() : nullptr;
   return mover ? mover->GetLastMoveDirection() : Vector3{ 0.0f, 0.0f, 1.0f };
}

void VehicleController::Update(float deltaTime) {
   if (!HasOwner()) { return; }

   // Inspectorで依存Componentが削除・再追加されるため、利用する更新ごとに解決する。
   auto* input = GetOwner().GetComponent<VehicleInputComponent>();
   auto* mover = GetOwner().GetComponent<VehicleMover>();
   auto* jump = GetOwner().GetComponent<CharacterJump>();

   // 現在の重力Upを取得（無ければワールドUp）
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   Vector3 gravityUp = { 0.0f, 1.0f, 0.0f };
   if (gravityBody) { gravityUp = gravityBody->GetTargetUpVector(); }

   // 制御側は物理キーを知らず、意味付け済みの入力だけを消費する。
   // カメラやその部品をInspectorで削除しても、次の入力で旧メモリへアクセスしない。
   auto* world = SceneWorld::GetCurrent();
   auto* camera = world ? world->FindVirtualCamera(gravityFollowCameraId_) : nullptr;
   auto* gravityFollowCamera = camera ? camera->GetComponent<GravityFollowCamera>() : nullptr;
   if (gravityFollowCamera && input) {
      const Vector2 camInput = input->GetCameraLookInput();
      if (camInput.x != 0.0f || camInput.y != 0.0f) {
         constexpr float kCamRotateScale = 150.0f;
         Vector2 delta = { camInput.x * kCamRotateScale * deltaTime,
                           camInput.y * kCamRotateScale * deltaTime };
         gravityFollowCamera->ProcessInput(delta, 0, true);
      }
   }

   // 接地判定（ジャンプ中でなければ接地扱い）
   bool isGrounded = !(jump && jump->IsJumping());

   // ジャンプ入力
   if (jump && input && input->IsJumpTriggered()) {
	  jump->Jump(gravityUp);
   }

   // 移動・姿勢入力を VehicleMover へ委譲
   if (mover) {
	  const float steerInput = input ? input->GetSteerInput() : 0.0f;
	  const float rollInput = input ? input->GetRollInput() : 0.0f;
	  const float pitchInput = input ? input->GetPitchInput() : 0.0f;
	  const bool driftInput = input && input->IsDriftHeld();
	  mover->ApplyMovement(steerInput, rollInput, pitchInput, driftInput, isGrounded, gravityUp, deltaTime);
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
