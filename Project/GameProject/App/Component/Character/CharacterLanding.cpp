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
   // オーナー不在時は処理しない
   if (!HasOwner()) { return; }

   // 必須コンポーネント取得
   auto* transform   = GetOwner().GetComponent<GameEngine::TransformComponent>();
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   if (!transform || !gravityBody) { return; }

   // 惑星中心からの距離と重力Upを算出
   GameEngine::Vector3 toSelf = transform->transform.translation - planetCenter_;
   float dist = toSelf.Length();
   if (dist < 1e-4f) { return; }

   GameEngine::Vector3 gravityUp = toSelf * (1.0f / dist);
   GameEngine::Vector3 vel       = gravityBody->GetVelocity();
   float upComp = vel.Dot(gravityUp);

   // ジャンプ状態を確認
   auto* jump      = GetOwner().GetComponent<CharacterJump>();
   bool  isJumping = jump && jump->IsJumping();

   if (isJumping) {
      // 落下中かつ地表到達で着地
      if (dist <= surfaceRadius_ && upComp <= 0.0f) {
         // 位置を地表にスナップ
         transform->transform.translation = planetCenter_ + gravityUp * surfaceRadius_;

         // 垂直速度のみ除去し、水平成分は維持
         vel = vel - gravityUp * upComp;
         gravityBody->SetVelocity(vel);

         // ジャンプ状態解除
         if (jump) { jump->NotifyLanded(); }

         // 歩行慣性も着地時にリセット
         if (auto* walker = GetOwner().GetComponent<CharacterWalker>()) {
            walker->ResetHorizontalVelocity();
         }
         isGrounded_ = true;
      }
   } else {
      // 非ジャンプ時は常に地表へ固定し、速度を完全停止
      transform->transform.translation = planetCenter_ + gravityUp * surfaceRadius_;
      gravityBody->SetVelocity({ 0.0f, 0.0f, 0.0f });
      isGrounded_ = true;
   }
}

#ifdef USE_IMGUI
void CharacterLanding::DrawInspector() {
   if (!ImGui::CollapsingHeader("CharacterLanding")) {
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
