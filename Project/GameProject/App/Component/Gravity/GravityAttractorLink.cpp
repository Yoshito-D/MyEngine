#include "GravityAttractorLink.h"
#include "GravityBody.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void GravityAttractorLink::Update(float) {
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
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Separator();
   ImGui::Text("%s: %s", Tr("アトラクター", "Attractor"), attractor_ ? Tr("設定済み", "Set") : Tr("なし", "None"));
}
#endif

} // namespace App
