#include "SphericalGravityAttractor.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"

namespace App {

void SphericalGravityAttractor::DrawInspector() {
   if (!ImGui::CollapsingHeader("SphericalGravityAttractor")) {
      return;
   }
   ImGui::Separator();

   // 影響半径を調整（0以下は無限範囲）
   ImGui::DragFloat("Influence Radius", &influenceRadius, 0.5f, 0.0f, 500.0f);
   if (influenceRadius <= 0.0f) {
      ImGui::SameLine();
      ImGui::TextDisabled("(Infinite)");
   }

   // 現在の中心座標を表示
   if (HasOwner()) {
      if (auto* t = GetOwner().GetComponent<GameEngine::TransformComponent>()) {
         auto& pos = t->transform.translation;
         ImGui::Text("Center: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
      }
   }
}

} // namespace App

#endif
