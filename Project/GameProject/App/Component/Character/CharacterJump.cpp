#include "CharacterJump.h"
#include "../Gravity/GravityBody.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#include <algorithm>
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
void CharacterJump::StartDebugVerticalDrop() {
   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   if (!transform || !gravityBody) {
	  return;
   }

   GameEngine::Vector3 gravityUp = gravityBody->GetTargetUpVector().Normalize();
   if (gravityUp.LengthSquared() < 1e-8f) {
	  gravityUp = gravityBody->GetCurrentUpVector().Normalize();
   }
   if (gravityUp.LengthSquared() < 1e-8f) {
	  gravityUp = { 0.0f, 1.0f, 0.0f };
   }

   const float dropHeight = std::max(0.0f, debugVerticalDropHeight_);
   const float initialDownwardSpeed = std::max(0.0f, debugVerticalDropInitialSpeed_);

   // 座標だけを動かすと CharacterLanding が非ジャンプ状態として地表へ戻すため、
   // 位置・垂直速度・ジャンプ状態を同じ操作で設定する。
   transform->transform.translation =
	  transform->transform.translation + gravityUp * dropHeight;
   gravityBody->SetVelocity(-gravityUp * initialDownwardSpeed);
   gravityBody->SetCurrentUpVector(gravityUp);
   gravityBody->SetTargetUpVector(gravityUp);
   isJumping_ = true;
}

void CharacterJump::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Separator();
   ImGui::DragFloat(Tr("ジャンプ力", "Jump Strength"), &jumpStrength, 0.1f, 0.0f, 30.0f);
   ImGui::Text("%s: %s", Tr("ジャンプ中", "Is Jumping"), isJumping_ ? Tr("はい", "true") : Tr("いいえ", "false"));

   ImGui::Separator();
   ImGui::TextDisabled("%s", Tr(
	  "計測用: 水平速度を0にし、現在のGravityUp方向へ移動して垂直落下を開始します。",
	  "Measurement: clears horizontal speed, moves along GravityUp, and starts a vertical drop."));
   ImGui::DragFloat(Tr("落下開始高度", "Drop Start Height"),
	  &debugVerticalDropHeight_, 0.5f, 1.0f, 200.0f, "%.1f");
   ImGui::DragFloat(Tr("初期下向き速度", "Initial Downward Speed"),
	  &debugVerticalDropInitialSpeed_, 0.5f, 0.0f, 100.0f, "%.1f");
   ImGui::BeginDisabled(isJumping_);
   if (ImGui::Button(Tr("垂直落下テスト開始", "Start Vertical Drop Test"))) {
	  StartDebugVerticalDrop();
   }
   ImGui::EndDisabled();
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
