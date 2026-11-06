#include "GameProject/App/Component/Character/CharacterLanding.h"
#include "GameEngine/Math/Functions/CollisionGeometry.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include "GameProject/App/Component/Character/CharacterJump.h"
#include "GameProject/App/Component/Character/CharacterWalker.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include <cmath>
#include "GameProject/App/Component/Gravity/PlanetSwitcher.h"
#include "GameProject/App/Component/Vehicle/VehicleEvents.h"

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace App {

void CharacterLanding::Update(float) {
   // オーナー不在時は処理しない
   if (!HasOwner()) return;

   // 必須コンポーネント取得
   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();

   if (!transform || !gravityBody) return;

   auto* jump = GetOwner().GetComponent<CharacterJump>();
   bool isJumping = jump && jump->IsJumping();
   auto* switcher = GetOwner().GetComponent<PlanetSwitcher>();

   GameEngine::Vector3 landingCenter = planetCenter_;
   float landingSurfaceRadius = surfaceRadius_;

   if (switcher && switcher->IsEnabled()) {
      if (!switcher->TryGetLandingPlanet(landingCenter, landingSurfaceRadius)) {
         isGrounded_ = false;
         return;
      }
   }

   // 惑星中心からの距離と重力Upを算出
   GameEngine::Vector3 toSelf = transform->GetLocalPose().translation - landingCenter;
   float dist = toSelf.Length();

   if (dist < 1e-4f) return;

   GameEngine::Vector3 gravityUp = toSelf * (1.0f / dist);
   float upComp = gravityBody->GetVelocity().Dot(gravityUp);

   // 判定とカメラ予測で同じOBB支持半径を使い、接触位置のずれを防ぐ。
   const float obbSupportRadius =
      GameEngine::ComputeObbSupportRadius(transform->GetLocalPose().GetActiveQuaternion(), obbHalfExtents, gravityUp);

   float snapRadius = landingSurfaceRadius + landingOffset + obbSupportRadius;

   if (isJumping) {
      // 落下中かつ地表到達で着地
      if (dist <= snapRadius && upComp <= 0.0f) {
         if (switcher) {
            switcher->CommitPendingSwitch();
         }

         // 位置を地表にスナップ
         {
            auto pose = transform->GetLocalPose();
            pose.translation = landingCenter + gravityUp * snapRadius;
            transform->ApplyLocalPose(pose);
         }

         // OBB の下端が惑星と接した点と、その地点の外向き法線を保存する。
         // landingOffset は実際の接地面として扱われているため接触点にも反映する。
         lastLandingContactPoint_ = landingCenter + gravityUp * (landingSurfaceRadius + landingOffset);
         lastLandingNormal_ = gravityUp;
         hasLandingContact_ = true;

         if (auto* events = GetOwner().GetComponent<VehicleEvents>()) {
            events->PublishLanding(lastLandingContactPoint_, lastLandingNormal_);
         }

         // 垂直速度のみ除去し、水平成分は維持
         gravityBody->CancelNormalVelocity(gravityUp);

         // 積分後の接触地点の法線を通知し、姿勢と着地品質の判定に同じ法線を使う。
         gravityBody->SetTargetUpVector(gravityUp);

         // ジャンプ状態解除
         if (jump) {
            jump->NotifyLanded();
         }

         // 歩行慣性も着地時にリセット
         if (auto* walker = GetOwner().GetComponent<CharacterWalker>()) {
            walker->ResetHorizontalVelocity();
         }

         isGrounded_ = true;
      } else {
         isGrounded_ = false;
      }
   } else {
      // 非ジャンプ時は常に地表へ固定し、速度を完全停止
      {
         auto pose = transform->GetLocalPose();
         pose.translation = landingCenter + gravityUp * snapRadius;
         transform->ApplyLocalPose(pose);
      }
      gravityBody->StopMotion();

      isGrounded_ = true;
   }
}

#ifdef USE_IMGUI
void CharacterLanding::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str())) return;

   ImGui::Separator();
   ImGui::DragFloat(Tr("地表半径", "Surface Radius"), &surfaceRadius_, 0.1f, 0.0f, 1000.0f);
   ImGui::DragFloat(Tr("着地オフセット", "Landing Offset"), &landingOffset, 0.01f, -100.0f, 100.0f);
   ImGui::DragFloat3(Tr("OBB半径", "OBB Half Extents"), &obbHalfExtents.x, 0.01f, 0.0f, 100.0f);
   ImGui::Text(
      "%s: (%.2f, %.2f, %.2f)", Tr("惑星中心", "Planet Center"), planetCenter_.x, planetCenter_.y, planetCenter_.z);
   ImGui::Text("%s: %s", Tr("接地中", "Is Grounded"), isGrounded_ ? Tr("はい", "true") : Tr("いいえ", "false"));
   Configure(DescribeSettings());
}
#endif

nlohmann::json CharacterLanding::Serialize() const {
   nlohmann::json json;
   json["surfaceRadius"] = surfaceRadius_;
   json["landingOffset"] = landingOffset;
   json["obbHalfExtents"]["x"] = obbHalfExtents.x;
   json["obbHalfExtents"]["y"] = obbHalfExtents.y;
   json["obbHalfExtents"]["z"] = obbHalfExtents.z;
   return json;
}

nlohmann::json CharacterLanding::GetSharedParameters() const {
   return ExtractSharedParameters({});
}

void CharacterLanding::Deserialize(const nlohmann::json& data) {
   ReadParameters(data, true);
}

void CharacterLanding::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data, false);
}

void CharacterLanding::ReadParameters(const nlohmann::json& data, [[maybe_unused]] bool initializeRuntime) {
   if (data.contains("surfaceRadius")) {
      surfaceRadius_ = data["surfaceRadius"];
   }

   if (data.contains("landingOffset")) {
      landingOffset = data["landingOffset"];
   }

   if (data.contains("obbHalfExtents")) {
      const auto& h = data["obbHalfExtents"];

      if (h.contains("x")) {
         obbHalfExtents.x = h["x"];
      }

      if (h.contains("y")) {
         obbHalfExtents.y = h["y"];
      }

      if (h.contains("z")) {
         obbHalfExtents.z = h["z"];
      }
   }

   Configure(DescribeSettings());
}

} // namespace App

namespace App {

CharacterLanding::Settings CharacterLanding::DescribeSettings() const {
   Settings settings;
   settings.landingOffset = landingOffset;
   settings.obbHalfExtents = obbHalfExtents;
   return settings;
}

void CharacterLanding::Configure(const Settings& requested) {
   auto settings = requested;
   [[maybe_unused]] const Settings defaults;

   if (!std::isfinite(settings.landingOffset)) {
      settings.landingOffset = defaults.landingOffset;
   }

   if (!std::isfinite(settings.obbHalfExtents.x)) {
      settings.obbHalfExtents.x = defaults.obbHalfExtents.x;
   }

   if (!std::isfinite(settings.obbHalfExtents.y)) {
      settings.obbHalfExtents.y = defaults.obbHalfExtents.y;
   }

   if (!std::isfinite(settings.obbHalfExtents.z)) {
      settings.obbHalfExtents.z = defaults.obbHalfExtents.z;
   }

   settings.obbHalfExtents.x = std::max(0.0f, settings.obbHalfExtents.x);
   settings.obbHalfExtents.y = std::max(0.0f, settings.obbHalfExtents.y);
   settings.obbHalfExtents.z = std::max(0.0f, settings.obbHalfExtents.z);
   landingOffset = settings.landingOffset;
   obbHalfExtents = settings.obbHalfExtents;
}

} // namespace App
