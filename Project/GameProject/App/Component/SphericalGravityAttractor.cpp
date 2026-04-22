#include "SphericalGravityAttractor.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"

namespace GameEngine {

void SphericalGravityAttractor::DrawInspector() {
   ImGui::Text("SphericalGravityAttractor Component");
   ImGui::Separator();

   ImGui::DragFloat("Influence Radius", &influenceRadius, 0.5f, 0.0f, 500.0f);
   if (influenceRadius <= 0.0f) {
	  ImGui::SameLine();
	  ImGui::TextDisabled("(Infinite)");
   }

   // 中心座標を表示
   if (HasOwner()) {
	  if (auto* transform = GetOwner().GetComponent<TransformComponent>()) {
		 auto& pos = transform->transform.translation;
		 ImGui::Text("Center: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
	  }
   }
}

} // namespace GameEngine

#endif
