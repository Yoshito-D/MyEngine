#include "MeshNormalGravityAttractor.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"

namespace GameEngine {

void MeshNormalGravityAttractor::DrawInspector() {
    ImGui::Text("MeshNormalGravityAttractor Component");
    ImGui::Separator();

    ImGui::Text("Fallback Up: (%.2f, %.2f, %.2f)",
        fallbackUpVector_.x, fallbackUpVector_.y, fallbackUpVector_.z);

    ImGui::TextDisabled("(Override FindSurfaceNormal() for full raycast)");
}

} // namespace GameEngine

#endif
