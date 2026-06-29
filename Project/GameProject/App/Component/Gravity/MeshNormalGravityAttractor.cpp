#include "MeshNormalGravityAttractor.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#include "Object/Component/IObjectComponent.h"

namespace App {

void MeshNormalGravityAttractor::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Separator();

   // 法線取得失敗時に使われる代替Upを表示
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("代替Up", "Fallback Up"),
      fallbackUpVector_.x, fallbackUpVector_.y, fallbackUpVector_.z);

   // 実運用では派生側で FindSurfaceNormal を実装する
   ImGui::TextDisabled("%s", Tr("(完全なレイキャストには FindSurfaceNormal() を override)", "(Override FindSurfaceNormal() for full raycast)"));
}

} // namespace App

#endif
