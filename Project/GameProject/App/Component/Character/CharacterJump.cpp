#include "CharacterJump.h"
#include "../Gravity/GravityBody.h"
#include "Object/Object.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void CharacterJump::Jump(const GameEngine::Vector3& gravityUp) {
   if (isJumping_) { return; }
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   if (!gravityBody) { return; }
   gravityBody->SetVelocity(gravityBody->GetVelocity() + gravityUp * jumpStrength);
   isJumping_ = true;
}

#ifdef USE_IMGUI
void CharacterJump::DrawInspector() {
   if (!ImGui::CollapsingHeader("CharacterJump")) {
      return;
   }
   ImGui::Separator();
   ImGui::DragFloat("Jump Strength", &jumpStrength, 0.1f, 0.0f, 30.0f);
   ImGui::Text("Is Jumping: %s", isJumping_ ? "true" : "false");
}
#endif

nlohmann::json CharacterJump::Serialize() const {
   nlohmann::json json;
   json["jumpStrength"] = jumpStrength;
   return json;
}

void CharacterJump::Deserialize(const nlohmann::json& data) {
   if (data.contains("jumpStrength")) { jumpStrength = data["jumpStrength"]; }
}

} // namespace App
