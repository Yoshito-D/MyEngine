#include "SphericalGravityAttractor.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"

namespace App {

void SphericalGravityAttractor::DrawInspector() {
   if (!ImGui::CollapsingHeader("SphericalGravityAttractor", ImGuiTreeNodeFlags_DefaultOpen)) {
      return;
   }
   ImGui::Separator();
   ImGui::DragFloat("Influence Radius", &influenceRadius, 0.5f, 0.0f, 500.0f);
   if (influenceRadius <= 0.0f) {
      ImGui::SameLine();
      ImGui::TextDisabled("(Infinite)");
   }
   if (HasOwner()) {
      if (auto* t = GetOwner().GetComponent<GameEngine::TransformComponent>()) {
         auto& pos = t->transform.translation;
         ImGui::Text("Center: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
      }
   }
}

} // namespace App

#endif
