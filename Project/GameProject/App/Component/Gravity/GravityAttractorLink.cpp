#include "GameProject/App/Component/Gravity/GravityAttractorLink.h"
#include "GameProject/App/Component/Gravity/PlanetSwitcher.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Object.h"

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace App {

void GravityAttractorLink::Update(float) {
   if (!HasOwner()) return;

   // 適用対象の GravityBody と現在位置を取得
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();

   if (!gravityBody || !transform) return;

   // 接続先が削除・無効化・圏外になった場合、前フレームの加速度を残さない。
   const auto* selector = GetOwner().GetComponent<PlanetSwitcher>();
   auto* attractor = selector ? selector->GetSelectedAttractor() : nullptr;

   if (!attractor || !attractor->ApplyTo(*gravityBody, transform->GetLocalPose().translation)) {
      gravityBody->ReleaseGravity();
   }
}

#ifdef USE_IMGUI
void GravityAttractorLink::DrawInspector() {
   const auto* selector = HasOwner() ? GetOwner().GetComponent<PlanetSwitcher>() : nullptr;
   ImGui::TextUnformatted(
      selector && selector->GetSelectedAttractor() ? "Gravity source selected" : "No gravity source");
}
#endif

} // namespace App
