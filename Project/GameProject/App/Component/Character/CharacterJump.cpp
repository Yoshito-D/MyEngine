#include "CharacterJump.h"
#include "../Gravity/GravityBody.h"
#include "Object/Object.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void CharacterJump::Jump(const GameEngine::Vector3& gravityUp) {
   // 空中での多重ジャンプを防止
   if (isJumping_) { return; }

   // 重力ボディへ上向き速度を加算
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   if (!gravityBody) { return; }
   gravityBody->SetVelocity(gravityBody->GetVelocity() + gravityUp * jumpStrength);

   // ジャンプ状態を記録
   isJumping_ = true;
}

#ifdef USE_IMGUI
void CharacterJump::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Separator();
   ImGui::DragFloat(Tr("ジャンプ力", "Jump Strength"), &jumpStrength, 0.1f, 0.0f, 30.0f);
   ImGui::Text("%s: %s", Tr("ジャンプ中", "Is Jumping"), isJumping_ ? Tr("はい", "true") : Tr("いいえ", "false"));
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
