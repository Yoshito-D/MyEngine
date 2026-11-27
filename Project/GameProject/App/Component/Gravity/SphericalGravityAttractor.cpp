#include "GameProject/App/Component/Gravity/SphericalGravityAttractor.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#endif

namespace App {

std::optional<PlanetSurface> PlanetSurface::From(const GameEngine::Object& planet) {
   if (!planet.GetComponent<GameEngine::TransformComponent>()) {
      return std::nullopt;
   }

   const auto world = planet.GetWorldMatrix();
   const auto center = GameEngine::TransformCoordinate({}, world);
   const GameEngine::Vector3 x{world.m[0][0], world.m[0][1], world.m[0][2]};
   const GameEngine::Vector3 y{world.m[1][0], world.m[1][1], world.m[1][2]};
   const GameEngine::Vector3 z{world.m[2][0], world.m[2][1], world.m[2][2]};

   // 軸が直交すると最大軸の長さになり、親の非一様スケールで軸が傾いた場合も球が地表を包含する。
   const float xy = std::abs(x.Dot(y));
   const float xz = std::abs(x.Dot(z));
   const float yz = std::abs(y.Dot(z));
   constexpr float kMeshRadius = 0.5f; ///< 拡縮前のメッシュ半径（m）。
   const float radius = kMeshRadius * std::sqrt(std::max(
      {x.LengthSquared() + xy + xz, y.LengthSquared() + xy + yz, z.LengthSquared() + xz + yz}));
   if (!std::isfinite(center.LengthSquared()) || !std::isfinite(radius) || radius <= 0.0f) {
      return std::nullopt;
   }

   return PlanetSurface{center, radius};
}

/// @brief 影響半径内かどうかを返す（0以下は無限範囲）
bool SphericalGravityAttractor::IsInRange(const GameEngine::Vector3& objectPosition) const {
   const auto surface = HasOwner() ? PlanetSurface::From(GetOwner()) : std::nullopt;
   return surface && (influenceRadius <= 0.0f ||
      (objectPosition - surface->center).LengthSquared() <= influenceRadius * influenceRadius);
}

/// @brief 中心から対象への方向を重力Upとして返す
GameEngine::Vector3 SphericalGravityAttractor::GetUpVectorFor(const GameEngine::Vector3& objectPosition) const {
   GameEngine::Vector3 dir = objectPosition - GetCenter();

   if (dir.LengthSquared() < 1e-8f) {
      return GameEngine::Vector3{0.0f, 1.0f, 0.0f};
   }

   return dir.Normalize();
}

#ifdef USE_IMGUI
void SphericalGravityAttractor::DrawInspector() {

   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) return;

   ImGui::Separator();

   // 影響範囲の球をシーンに描画
   GameEngine::EngineContext::DrawSphere(
      GetCenter(), influenceRadius > 0.0f ? influenceRadius : 1.0f, GameEngine::Vector4{0.1f, 0.5f, 1.0f, 1.0f}, false);

   // 影響半径を調整（0以下は無限範囲）
   ImGui::DragFloat(Tr("影響半径", "Influence Radius"), &influenceRadius, 0.5f, 0.0f, 500.0f);

   if (influenceRadius <= 0.0f) {
      ImGui::SameLine();
      ImGui::TextDisabled("%s", Tr("(無限)", "(Infinite)"));
   }

   // 現在の中心座標を表示
   const auto pos = GetCenter();
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("中心", "Center"), pos.x, pos.y, pos.z);

   Configure(DescribeSettings());
}
#endif

} // namespace App

namespace App {

nlohmann::json SphericalGravityAttractor::Serialize() const {
   nlohmann::json json;
   json["influenceRadius"] = influenceRadius;
   return json;
}

nlohmann::json SphericalGravityAttractor::GetSharedParameters() const {
   return ExtractSharedParameters({});
}

void SphericalGravityAttractor::Deserialize(const nlohmann::json& data) {
   ReadParameters(data);
}

void SphericalGravityAttractor::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data);
}

void SphericalGravityAttractor::ReadParameters(const nlohmann::json& data) {
   if (data.contains("influenceRadius")) {
      influenceRadius = data["influenceRadius"];
   }
}

SphericalGravityAttractor::Settings SphericalGravityAttractor::DescribeSettings() const {
   Settings settings;
   settings.influenceRadius = influenceRadius;
   return settings;
}

void SphericalGravityAttractor::Configure(const Settings& requested) {
   auto settings = requested;
   [[maybe_unused]] const Settings defaults;

   if (!std::isfinite(settings.influenceRadius)) {
      settings.influenceRadius = defaults.influenceRadius;
   }

   settings.influenceRadius = std::max(0.0f, settings.influenceRadius);
   influenceRadius = settings.influenceRadius;
}

GameEngine::Vector3 SphericalGravityAttractor::GetCenter() const {
   if (!HasOwner()) {
      return {0.0f, 0.0f, 0.0f};
   }

   const auto surface = PlanetSurface::From(GetOwner());
   return surface ? surface->center : GameEngine::Vector3{};
}

} // namespace App
