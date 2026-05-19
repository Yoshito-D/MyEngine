#include "GravityAttractorLink.h"
#include "GravityBody.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void GravityAttractorLink::Update(float /*deltaTime*/) {
   // オーナーまたはアトラクタ未設定時は処理しない
   if (!HasOwner() || !attractor_) { return; }

   // 適用対象の GravityBody と現在位置を取得
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   auto* transform   = GetOwner().GetComponent<GameEngine::TransformComponent>();
   if (!gravityBody || !transform) { return; }

   // 発生源側ロジックで重力を適用
   attractor_->ApplyTo(*gravityBody, transform->transform.translation);
}

#ifdef USE_IMGUI
void GravityAttractorLink::DrawInspector() {
   if (!ImGui::CollapsingHeader("GravityAttractorLink")) {
      return;
   }
   ImGui::Separator();
   ImGui::Text("Attractor: %s", attractor_ ? "Set" : "None");
}
#endif

} // namespace App
