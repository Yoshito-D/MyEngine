#include "VehicleAirController.h"
#include "../Gravity/GravityBody.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"
#include "Utility/MathUtils/MathConstants.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

using namespace GameEngine;

namespace App {
namespace {

constexpr float kMaxControlDeltaTime = 0.05f;
constexpr float kInputNoiseThreshold = 0.02f;
constexpr float kAngularVelocityEpsilon = 0.001f;

} // namespace

// ---------------------------------------------------------------
// public
// ---------------------------------------------------------------

void VehicleAirController::Apply(float rollInput, float pitchInput, float deltaTime) {
   auto* transform = GetOwner().GetComponent<TransformComponent>();
   if (!transform) { return; }

   // 処理落ちやブレーク復帰時の大きなdtで、慣性が1フレームで消えることを防ぐ。
   const float controlDeltaTime = std::clamp(deltaTime, 0.0f, kMaxControlDeltaTime);

   const Quaternion currentRot = transform->transform.GetActiveQuaternion();

   // 角速度を目標値に向けて更新する。
   // スティックを戻す途中の小さい入力で、保持中の角速度を直接上書きしない。
   UpdateAngularVelocity(-rollInput, angularVelRoll_,
						 -rollInput * rollSpeed * MathConstants::kDegreesToRadians, controlDeltaTime);
   UpdateAngularVelocity(pitchInput, angularVelPitch_,
						 pitchInput * pitchSpeed * MathConstants::kDegreesToRadians, controlDeltaTime);

   // roll後の姿勢からpitch軸を取り直し、同時入力時も常に機体のローカル軸で回す。
   Quaternion newRot = currentRot;
   const Vector3 localForward = RotateVector({ 0.0f, 0.0f, 1.0f }, newRot);
   ApplyRollRotation(newRot, localForward, controlDeltaTime);
   const Vector3 localRight = RotateVector({ 1.0f, 0.0f, 0.0f }, newRot);
   ApplyPitchRotation(newRot, localRight, controlDeltaTime);

   // 角速度がほぼゼロのときは transform への書き込みをスキップして
   // 浮動小数点の累積誤差による不要な更新を防ぐ。
   if (std::abs(angularVelRoll_) > 1e-6f || std::abs(angularVelPitch_) > 1e-6f) {
	  transform->transform.SetRotationQuaternion(newRot);
   }

   // 空中でのわずかな水平減速（空気抵抗的な演出）。
   // GravityBody の速度から垂直成分（重力方向）を分離し、水平成分だけに
   // exp(-airDrag * dt) の減衰を掛けて戻す。垂直成分は重力計算に任せる。
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   if (gravityBody) {
	  const Vector3& vel = gravityBody->GetVelocity();
	  const Vector3& up  = gravityBody->GetCurrentUpVector();
	  float   vertSpeed  = vel.Dot(up);
	  Vector3 vertical   = up * vertSpeed;
	  Vector3 horizontal = vel - vertical;
	  float   drag       = std::exp(-airDrag * controlDeltaTime);
	  gravityBody->SetVelocity(horizontal * drag + vertical);
   }
}

// ---------------------------------------------------------------
// private
// ---------------------------------------------------------------

void VehicleAirController::UpdateAngularVelocity(float input, float& angVel,
												 float targetVel, float deltaTime) const {
   const bool hasInput = std::abs(input) > kInputNoiseThreshold;
   const bool reversesDirection = angVel * targetVel < 0.0f;
   const bool acceleratesCurrentDirection = std::abs(targetVel) >= std::abs(angVel);

   if (hasInput && (reversesDirection || acceleratesCurrentDirection)) {
	  // 新規入力・加速・反転は即時反映し、操作の応答性を維持する。
	  angVel = targetVel;
   } else {
	  // 入力解放または同方向への弱い入力では、現在の運動量から指数減衰させる。
	  // アナログスティックの戻り途中に発生する小さい値で、慣性を上書きしないための分岐。
	  angVel *= std::exp(-std::max(angularDamping, 0.0f) * deltaTime);

	  // 弱い入力を保持している場合は、その目標角速度を下回らないよう収束させる。
	  if (hasInput && std::abs(angVel) < std::abs(targetVel)) {
		 angVel = targetVel;
	  } else if (!hasInput && std::abs(angVel) < kAngularVelocityEpsilon) {
		 angVel = 0.0f;
	  }
   }
}

void VehicleAirController::ApplyPitchRotation(Quaternion& rot,
											  const Vector3& localRight, float deltaTime) const {
   if (std::abs(angularVelPitch_) <= 1e-6f) { return; }

   // localRight 軸まわりの pitch 回転（前後傾き）を合成する。
   // roll と同じく左掛けでワールド空間基準の軸を維持する。
   Quaternion delta = MakeRotateAxisAngleQuaternion(localRight, angularVelPitch_ * deltaTime);
   rot = (delta * rot).Normalize();
}

void VehicleAirController::ApplyRollRotation(GameEngine::Quaternion& rot, 
                                             const Vector3& localForward, float deltaTime) const {
   if (std::abs(angularVelRoll_) <= 1e-6f) { return; }

   // localForward 軸まわりの roll 回転（左右傾き）を合成する。
   Quaternion delta = MakeRotateAxisAngleQuaternion(localForward, angularVelRoll_ * deltaTime);
   rot = (delta * rot).Normalize();
}

// ---------------------------------------------------------------
// ImGui / Serialize
// ---------------------------------------------------------------

#ifdef USE_IMGUI
void VehicleAirController::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) { return; }
   ImGui::Separator();
   ImGui::DragFloat((std::string(Tr("ロール速度 (deg/s)", "Roll Speed (deg/s)")) + "##1").c_str(), &rollSpeed, 1.0f, 0.0f, 360.0f);
   ImGui::DragFloat(Tr("ピッチ速度 (deg/s)", "Pitch Speed (deg/s)"), &pitchSpeed, 1.0f, 0.0f, 360.0f);
   ImGui::DragFloat(Tr("角速度減衰", "Angular Damping"), &angularDamping, 0.1f, 0.1f,  20.0f);
   ImGui::DragFloat(Tr("空気抵抗", "Air Drag"),        &airDrag,        0.05f, 0.0f,  5.0f);
   ImGui::Spacing();
   ImGui::Text("%s (deg/s): %.2f / %.2f", Tr("角速度 Roll/Pitch", "AngVel Roll/Pitch"),
      angularVelRoll_ * MathConstants::kRadiansToDegrees,
      angularVelPitch_ * MathConstants::kRadiansToDegrees);
}
#endif

nlohmann::json VehicleAirController::Serialize() const {
   nlohmann::json json;
   json["steerSpeed"]     = rollSpeed;
   json["pitchSpeed"]     = pitchSpeed;
   json["angularDamping"] = angularDamping;
   json["airDrag"]        = airDrag;
   return json;
}

void VehicleAirController::Deserialize(const nlohmann::json& data) {
   if (data.contains("steerSpeed"))     { rollSpeed     = data["steerSpeed"]; }
   if (data.contains("pitchSpeed"))     { pitchSpeed     = data["pitchSpeed"]; }
   if (data.contains("angularDamping")) { angularDamping = data["angularDamping"]; }
   if (data.contains("airDrag"))        { airDrag        = data["airDrag"]; }
}

} // namespace App
