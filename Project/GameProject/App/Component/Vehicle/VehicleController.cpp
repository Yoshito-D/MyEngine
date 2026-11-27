#include "GameProject/App/Component/Vehicle/VehicleController.h"
#include "GameEngine/Object/Object.h"
#include "GameProject/App/Component/Character/CharacterJump.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameProject/App/Component/Vehicle/VehicleInputComponent.h"
#include "GameProject/App/Component/Vehicle/VehicleMover.h"

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

using namespace GameEngine;

namespace App {

void VehicleController::Update(float deltaTime) {
   if (!HasOwner()) return;

   // Inspectorで依存Componentが削除・再追加されるため、利用する更新ごとに解決する。
   auto* input = GetOwner().GetComponent<VehicleInputComponent>();
   auto* mover = GetOwner().GetComponent<VehicleMover>();
   auto* jump = GetOwner().GetComponent<CharacterJump>();

   // 現在の重力Upを取得（無ければワールドUp）
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   Vector3 gravityUp = {0.0f, 1.0f, 0.0f};

   if (gravityBody && gravityBody->IsEnabled()) {
      gravityUp = gravityBody->GetTargetUpVector();
   }

   // ジャンプ入力
   if (jump && jump->IsEnabled() && input && input->IsJumpTriggered()) {
      jump->Jump(gravityUp);
   }

   // 同じフレームに離陸した場合は、地上の操舵やドリフトを適用しない。
   const bool isGrounded = !(jump && jump->IsJumping());

   // 移動・姿勢入力を VehicleMover へ委譲
   if (mover && mover->IsEnabled()) {
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

   if (!ImGui::CollapsingHeader(header.c_str())) return;

   ImGui::Separator();
   ImGui::TextUnformatted(
      Tr("入力は VehicleInputComponent から取得します", "Input is provided by VehicleInputComponent"));
}
#endif

nlohmann::json VehicleController::Serialize() const {
   return nlohmann::json::object();
}

void VehicleController::Deserialize(const nlohmann::json& data) {
   (void)data;
}

} // namespace App
