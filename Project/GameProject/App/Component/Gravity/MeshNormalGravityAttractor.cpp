#include "MeshNormalGravityAttractor.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"

namespace App {

void MeshNormalGravityAttractor::DrawInspector() {
   if (!ImGui::CollapsingHeader("MeshNormalGravityAttractor")) {
      return;
   }
   ImGui::Separator();

   // 法線取得失敗時に使われる代替Upを表示
   ImGui::Text("Fallback Up: (%.2f, %.2f, %.2f)",
      fallbackUpVector_.x, fallbackUpVector_.y, fallbackUpVector_.z);

   // 実運用では派生側で FindSurfaceNormal を実装する
   ImGui::TextDisabled("(Override FindSurfaceNormal() for full raycast)");
}

} // namespace App

#endif
