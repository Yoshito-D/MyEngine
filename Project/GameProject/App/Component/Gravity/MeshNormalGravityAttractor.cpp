#include "GameProject/App/Component/Gravity/MeshNormalGravityAttractor.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#include "GameEngine/Object/Component/Base/IObjectComponent.h"

namespace App {

void MeshNormalGravityAttractor::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str())) return;

   ImGui::Separator();

   // 法線取得失敗時に使われる代替Upを表示
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("代替Up", "Fallback Up"), fallbackUpVector_.x, fallbackUpVector_.y,
      fallbackUpVector_.z);

   // 実運用では派生側で FindSurfaceNormal を実装する
   ImGui::TextDisabled("%s", Tr("(完全なレイキャストには FindSurfaceNormal() を override)",
                                "(Override FindSurfaceNormal() for full raycast)"));
}

} // namespace App

#endif

namespace App {

GameEngine::Vector3 MeshNormalGravityAttractor::GetUpVectorFor(const GameEngine::Vector3& objectPosition) const {
   GameEngine::Vector3 normal = FindSurfaceNormal(objectPosition);

   if (normal.LengthSquared() > 1e-8f) {
      return normal.Normalize();
   }

   return fallbackUpVector_;
}

void MeshNormalGravityAttractor::SetFallbackUpVector(const GameEngine::Vector3& up) {
   const float length = std::hypot(up.x, up.y, up.z);

   if (!std::isfinite(length) || length <= 1.0e-4f) return;

   fallbackUpVector_ = up / length;
}

nlohmann::json MeshNormalGravityAttractor::Serialize() const {
   nlohmann::json json;
   json["fallbackUp"] = {fallbackUpVector_.x, fallbackUpVector_.y, fallbackUpVector_.z};
   return json;
}

nlohmann::json MeshNormalGravityAttractor::GetSharedParameters() const {
   return ExtractSharedParameters({});
}

void MeshNormalGravityAttractor::Deserialize(const nlohmann::json& data) {
   ReadParameters(data, true);
}

void MeshNormalGravityAttractor::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data, false);
}

void MeshNormalGravityAttractor::ReadParameters(const nlohmann::json& data, [[maybe_unused]] bool initializeRuntime) {
   if (data.contains("fallbackUp")) {
      const auto& up = data.at("fallbackUp");

      if (up.is_array() && up.size() == 3 && up[0].is_number() && up[1].is_number() && up[2].is_number()) {
         SetFallbackUpVector({up[0].get<float>(), up[1].get<float>(), up[2].get<float>()});
      }
   }
}

GameEngine::Vector3 MeshNormalGravityAttractor::FindSurfaceNormal(const GameEngine::Vector3& /*objectPosition*/) const {
   return {0.0f, 0.0f, 0.0f};
}

} // namespace App
