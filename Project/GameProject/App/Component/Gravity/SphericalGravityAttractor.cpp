#include "SphericalGravityAttractor.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#include "EngineContext.h"
#include "Object/Component/IObjectComponent.h"
#endif

namespace App {

/// @brief 影響半径内かどうかを返す（0以下は無限範囲）
bool SphericalGravityAttractor::IsInRange(const GameEngine::Vector3& objectPosition) const {
   if (influenceRadius <= 0.0f) { return true; }
   GameEngine::Vector3 diff = objectPosition - GetCenter();
   return diff.LengthSquared() <= influenceRadius * influenceRadius;
}

/// @brief 中心から対象への方向を重力Upとして返す
GameEngine::Vector3 SphericalGravityAttractor::GetUpVectorFor(const GameEngine::Vector3& objectPosition) const {
   GameEngine::Vector3 dir = objectPosition - GetCenter();
   if (dir.LengthSquared() < 1e-8f) { return GameEngine::Vector3{ 0.0f, 1.0f, 0.0f }; }
   return dir.Normalize();
}

#ifdef USE_IMGUI
void SphericalGravityAttractor::DrawInspector() {

   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
      return;
   }

   ImGui::Separator();

   // 影響範囲の球をシーンに描画
   GameEngine::EngineContext::DrawSphere(GetCenter(), influenceRadius > 0.0f ? influenceRadius : 1.0f, GameEngine::Vector4{ 0.1f, 0.5f, 1.0f, 1.0f }, false);

   // 影響半径を調整（0以下は無限範囲）
   ImGui::DragFloat(Tr("影響半径", "Influence Radius"), &influenceRadius, 0.5f, 0.0f, 500.0f);
   if (influenceRadius <= 0.0f) {
      ImGui::SameLine();
      ImGui::TextDisabled("%s", Tr("(無限)", "(Infinite)"));
   }

   // 現在の中心座標を表示
   if (HasOwner()) {
      if (auto* t = GetOwner().GetComponent<GameEngine::TransformComponent>()) {
         auto& pos = t->transform.translation;
         ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("中心", "Center"), pos.x, pos.y, pos.z);
      }
   }

}
#endif

} // namespace App
