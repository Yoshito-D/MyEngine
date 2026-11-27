#include "GameProject/App/Component/Character/CharacterLanding.h"
#include "GameEngine/Math/Functions/CollisionGeometry.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include "GameEngine/Utility/Logger.h"
#include "GameProject/App/Component/Character/CharacterWalker.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include <cmath>
#include "GameProject/App/Component/Gravity/PlanetSwitcher.h"
#include "GameProject/App/Component/Vehicle/VehicleEvents.h"
#include "GameProject/App/Component/Vehicle/VehicleMover.h"
#include "GameProject/App/Component/Vehicle/VehicleController.h"

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace App {

void CharacterLanding::Update(float) {
   landedThisFrame_ = false;

   if (!HasOwner()) return;

   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   auto* body = GetOwner().GetComponent<GravityBody>();
   auto* switcher = GetOwner().GetComponent<PlanetSwitcher>();
   float radius = 0.0f;
   GameEngine::Vector3 center{};

   const bool initialPlacement = !evaluatedContact_;
   evaluatedContact_ = true;

   if (!transform || !body || !body->HasSurfaceGravity() || !switcher ||
       !switcher->TryGetLandingPlanet(center, radius)) {
      contact_.reset();
      return;
   }

   const auto* controller = GetOwner().GetComponent<VehicleController>();
   if (controller && controller->IsMotionStopped() && !initialPlacement) return;

   const auto position = GameEngine::TransformCoordinate({}, GetOwner().GetWorldMatrix());
   const auto radial = position - center;
   const float distance = radial.Length();
   constexpr float kMinNormalDistance = 1.0e-4f; ///< 法線を求める距離の下限（m）。

   if (distance < kMinNormalDistance) {
      contact_.reset();
      return;
   }

   const auto normal = radial / distance;
   const float support = GameEngine::ComputeObbSupportRadius(
      GetOwner().GetWorldRotation(), settings_.obbHalfExtents, normal);
   const float contactRadius = radius + settings_.landingOffset;
   const float snapRadius = contactRadius + support;
   const bool keepContact = IsGrounded() && !switcher->HasPendingPlanetChange();

   // 支持を失った後は、候補を再取得しただけでは地表に移動させない。
   if (!initialPlacement && !keepContact && (distance > snapRadius || body->GetVelocity().Dot(normal) > 0.0f)) {
      contact_.reset();
      return;
   }

   auto pose = transform->GetLocalPose();
   const auto worldToParent = GetOwner().GetParentWorldMatrix().Inverse();
   pose.translation = GameEngine::TransformCoordinate(center + normal * snapRadius, worldToParent);

   if (!pose.IsFinite()) {
      Logger::Fail("Cannot place vehicle on planet: " + GetOwner().GetEntityId(), Logger::LogChannel::Game);
   }
   transform->ApplyLocalPose(pose);
   switcher->CommitPendingSwitch();
   contact_ = SurfaceContact{center + normal * contactRadius, normal};
   body->SetTargetUpVector(normal);

   if (initialPlacement || keepContact) {
      body->StopMotion();
      return;
   }

   landedThisFrame_ = true;
   body->CancelNormalVelocity(normal);
   if (auto* walker = GetOwner().GetComponent<CharacterWalker>()) {
      walker->ResetHorizontalVelocity();
   }

   // 速度補正・姿勢と着地品質を確定してから、一つの通知として公開する。
   auto result = LandingResult::Normal;
   if (auto* mover = GetOwner().GetComponent<VehicleMover>();
       mover && mover->IsEnabled() && controller && controller->IsEnabled()) {
      result = mover->ResolveLanding(normal);
   }
   if (auto* events = GetOwner().GetComponent<VehicleEvents>(); events && events->IsEnabled()) {
      events->PublishLanding(*contact_, result);
   }
}

std::optional<SurfaceContact> CharacterLanding::Takeoff() {
   auto previous = contact_;
   contact_.reset();
   return previous;
}

void CharacterLanding::OnDisable() {
   contact_.reset();
}

#ifdef USE_IMGUI
void CharacterLanding::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str())) return;

   ImGui::Separator();
   auto settings = DescribeSettings();
   ImGui::DragFloat(Tr("着地オフセット (m)", "Landing Offset (m)"), &settings.landingOffset, 0.01f, -100.0f, 100.0f);
   ImGui::DragFloat3(Tr("OBB半サイズ (m)", "OBB Half Extents (m)"), &settings.obbHalfExtents.x, 0.01f, 0.0f, 100.0f);
   ImGui::Text("%s: %s", Tr("接地中", "Is Grounded"), IsGrounded() ? Tr("はい", "true") : Tr("いいえ", "false"));
   Configure(settings);
}
#endif

nlohmann::json CharacterLanding::Serialize() const {
   nlohmann::json json;
   json["landingOffset"] = settings_.landingOffset;
   json["obbHalfExtents"]["x"] = settings_.obbHalfExtents.x;
   json["obbHalfExtents"]["y"] = settings_.obbHalfExtents.y;
   json["obbHalfExtents"]["z"] = settings_.obbHalfExtents.z;
   return json;
}

nlohmann::json CharacterLanding::GetSharedParameters() const {
   return ExtractSharedParameters({});
}

void CharacterLanding::Deserialize(const nlohmann::json& data) {
   ReadParameters(data);
}

void CharacterLanding::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data);
}

void CharacterLanding::ReadParameters(const nlohmann::json& data) {
   auto settings = DescribeSettings();
   if (data.contains("landingOffset")) {
      settings.landingOffset = data.at("landingOffset").get<float>();
   }
   if (data.contains("obbHalfExtents")) {
      const auto& extents = data.at("obbHalfExtents");
      settings.obbHalfExtents = {extents.at("x").get<float>(), extents.at("y").get<float>(), extents.at("z").get<float>()};
   }
   Configure(settings);
}

CharacterLanding::Settings CharacterLanding::DescribeSettings() const {
   return settings_;
}

void CharacterLanding::Configure(const Settings& requested) {
   const auto& extents = requested.obbHalfExtents;
   if (!std::isfinite(requested.landingOffset) || !std::isfinite(extents.LengthSquared()) ||
       extents.x < 0.0f || extents.y < 0.0f || extents.z < 0.0f) {
      Logger::Fail("Invalid CharacterLanding landingOffset or obbHalfExtents", Logger::LogChannel::Game);
   }
   settings_ = requested;
}

} // namespace App
