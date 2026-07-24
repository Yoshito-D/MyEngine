#include "CharacterLanding.h"
#include "CharacterJump.h"
#include "CharacterWalker.h"
#include "../Gravity/GravityBody.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include <cmath>
#include "../Gravity/PlanetSwitcher.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void CharacterLanding::Update(float) {
   // オーナー不在時は処理しない
   if (!HasOwner()) { return; }

   // 必須コンポーネント取得
   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   if (!transform || !gravityBody) { return; }

   auto* jump = GetOwner().GetComponent<CharacterJump>();
   bool  isJumping = jump && jump->IsJumping();
   auto* switcher = GetOwner().GetComponent<PlanetSwitcher>();

   GameEngine::Vector3 landingCenter = planetCenter_;
   float landingSurfaceRadius = surfaceRadius_;
   if (isJumping && switcher) {
	  switcher->TryGetLandingPlanet(landingCenter, landingSurfaceRadius);
   }

   // 惑星中心からの距離と重力Upを算出
   GameEngine::Vector3 toSelf = transform->transform.translation - landingCenter;
   float dist = toSelf.Length();
   if (dist < 1e-4f) { return; }

   GameEngine::Vector3 gravityUp = toSelf * (1.0f / dist);
   GameEngine::Vector3 vel = gravityBody->GetVelocity();
   float upComp = vel.Dot(gravityUp);

   // OBBの支持半径を計算：重力Down方向へのOBBの最大射影長
   // = |dot(axisX, -gravityUp)| * halfX + |dot(axisY, -gravityUp)| * halfY + |dot(axisZ, -gravityUp)| * halfZ
   float obbSupportRadius = 0.0f;
   {
	  const GameEngine::Quaternion rot = transform->transform.GetActiveQuaternion();
	  const GameEngine::Vector3    half = obbHalfExtents;

	  // OBBの3軸をワールド空間に変換
	  GameEngine::Vector3 axisX = RotateVector({ 1.0f, 0.0f, 0.0f }, rot);
	  GameEngine::Vector3 axisY = RotateVector({ 0.0f, 1.0f, 0.0f }, rot);
	  GameEngine::Vector3 axisZ = RotateVector({ 0.0f, 0.0f, 1.0f }, rot);

	  // 各軸の重力Down方向への射影絶対値 × 半サイズ
	  obbSupportRadius = std::abs(axisX.Dot(-gravityUp)) * half.x
		 + std::abs(axisY.Dot(-gravityUp)) * half.y
		 + std::abs(axisZ.Dot(-gravityUp)) * half.z;
   }

   float snapRadius = landingSurfaceRadius + landingOffset + obbSupportRadius;

   if (isJumping) {
	  // 落下中かつ地表到達で着地
	  if (dist <= snapRadius && upComp <= 0.0f) {
		 if (switcher) {
			switcher->CommitPendingSwitch();
		 }

		 // 位置を地表にスナップ
		 transform->transform.translation = landingCenter + gravityUp * snapRadius;

		 // OBB の下端が惑星と接した点と、その地点の外向き法線を保存する。
		 // landingOffset は実際の接地面として扱われているため接触点にも反映する。
		 lastLandingContactPoint_ =
			landingCenter + gravityUp * (landingSurfaceRadius + landingOffset);
		 lastLandingNormal_ = gravityUp;
		 hasLandingContact_ = true;

		 // 垂直速度のみ除去し、水平成分は維持
		 vel = vel - gravityUp * upComp;
		 gravityBody->SetVelocity(vel);
		 // PlanetSwitcher の確定は GravityAttractorLink の更新後に起きるため、
		 // 同フレームの着地結果判定が古い惑星法線を参照しないよう同期する。
		 gravityBody->SetTargetUpVector(gravityUp);

		 // ジャンプ状態解除
		 if (jump) { jump->NotifyLanded(); }

		 // 歩行慣性も着地時にリセット
		 if (auto* walker = GetOwner().GetComponent<CharacterWalker>()) {
			walker->ResetHorizontalVelocity();
		 }

		 if (switcher) {
			if (switcher->HasSwitched()) {
			   switcher->ResetSwitchedFlag();
			}
		 }

		 isGrounded_ = true;
	  } else {
		 isGrounded_ = false;
	  }
   } else {
	  // 非ジャンプ時は常に地表へ固定し、速度を完全停止
	  transform->transform.translation = landingCenter + gravityUp * snapRadius;
	  gravityBody->SetVelocity({ 0.0f, 0.0f, 0.0f });

	  if (switcher) {
		 if (switcher->HasSwitched()) {
			switcher->ResetSwitchedFlag();
		 }
	  }

	  isGrounded_ = true;
   }
}

#ifdef USE_IMGUI
void CharacterLanding::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
	  return;
   }
   ImGui::Separator();
   ImGui::DragFloat(Tr("地表半径", "Surface Radius"), &surfaceRadius_, 0.1f, 0.0f, 1000.0f);
   ImGui::DragFloat(Tr("着地オフセット", "Landing Offset"), &landingOffset, 0.01f, -100.0f, 100.0f);
   ImGui::DragFloat3(Tr("OBB半径", "OBB Half Extents"), &obbHalfExtents.x, 0.01f, 0.0f, 100.0f);
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("惑星中心", "Planet Center"),
	  planetCenter_.x, planetCenter_.y, planetCenter_.z);
   ImGui::Text("%s: %s", Tr("接地中", "Is Grounded"), isGrounded_ ? Tr("はい", "true") : Tr("いいえ", "false"));
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

void CharacterLanding::Deserialize(const nlohmann::json& data) {
   if (data.contains("surfaceRadius")) { surfaceRadius_ = data["surfaceRadius"]; }
   if (data.contains("landingOffset")) { landingOffset = data["landingOffset"]; }
   if (data.contains("obbHalfExtents")) {
	  const auto& h = data["obbHalfExtents"];
	  if (h.contains("x")) { obbHalfExtents.x = h["x"]; }
	  if (h.contains("y")) { obbHalfExtents.y = h["y"]; }
	  if (h.contains("z")) { obbHalfExtents.z = h["z"]; }
   }
}

} // namespace App
