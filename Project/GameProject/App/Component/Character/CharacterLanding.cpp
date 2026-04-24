#include "CharacterLanding.h"
#include "CharacterJump.h"
#include "CharacterWalker.h"
#include "../Gravity/GravityBody.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void CharacterLanding::Update(float /*deltaTime*/) {
   if (!HasOwner()) { return; }

   auto* transform   = GetOwner().GetComponent<GameEngine::TransformComponent>();
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   if (!transform || !gravityBody) { return; }

   GameEngine::Vector3 toSelf = transform->transform.translation - planetCenter_;
   float dist = toSelf.Length();
   if (dist < 1e-4f) { return; }

   GameEngine::Vector3 gravityUp = toSelf * (1.0f / dist);
   GameEngine::Vector3 vel       = gravityBody->GetVelocity();
   float upComp = vel.Dot(gravityUp);

   auto* jump      = GetOwner().GetComponent<CharacterJump>();
   bool  isJumping = jump && jump->IsJumping();

   if (isJumping) {
      // 落下中かつ表面以下に到達したら着地
      if (dist <= surfaceRadius_ && upComp <= 0.0f) {
         transform->transform.translation = planetCenter_ + gravityUp * surfaceRadius_;
         // GravityBody の垂直速度を除去（水平は残す）
         vel = vel - gravityUp * upComp;
         gravityBody->SetVelocity(vel);
         if (jump) { jump->NotifyLanded(); }
         // CharacterWalker の慣性もリセット
         if (auto* walker = GetOwner().GetComponent<CharacterWalker>()) {
            walker->ResetHorizontalVelocity();
         }
         isGrounded_ = true;
      }
   } else {
      // 地上: 表面に固定し速度を完全にゼロ（慣性リセット）
      transform->transform.translation = planetCenter_ + gravityUp * surfaceRadius_;
      gravityBody->SetVelocity({ 0.0f, 0.0f, 0.0f });
      isGrounded_ = true;
   }
}

#ifdef USE_IMGUI
void CharacterLanding::DrawInspector() {
   if (!ImGui::CollapsingHeader("CharacterLanding", ImGuiTreeNodeFlags_DefaultOpen)) {
      return;
   }
   ImGui::Separator();
   ImGui::DragFloat("Surface Radius", &surfaceRadius_, 0.1f, 0.0f, 1000.0f);
   ImGui::Text("Planet Center: (%.2f, %.2f, %.2f)",
      planetCenter_.x, planetCenter_.y, planetCenter_.z);
   ImGui::Text("Is Grounded: %s", isGrounded_ ? "true" : "false");
}
#endif

nlohmann::json CharacterLanding::Serialize() const {
   nlohmann::json json;
   json["surfaceRadius"] = surfaceRadius_;
   return json;
}

void CharacterLanding::Deserialize(const nlohmann::json& data) {
   if (data.contains("surfaceRadius")) { surfaceRadius_ = data["surfaceRadius"]; }
}

} // namespace App
