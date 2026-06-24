#include "VehicleAirController.h"
#include "../Gravity/GravityBody.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

using namespace GameEngine;

namespace App {

// ---------------------------------------------------------------
// public
// ---------------------------------------------------------------

void VehicleAirController::Apply(float steerInput, float pitchInput, float deltaTime) {
   auto* transform = GetOwner().GetComponent<TransformComponent>();
   if (!transform) { return; }

   constexpr float kDeg2Rad = static_cast<float>(std::numbers::pi) / 180.0f;

   // 現在のクォータニオンからローカル軸を取得する。
   // localForward = (0,0,1) を現在回転で変換 → 機体の上方向（rool 軸として使う）
   // localRight   = (1,0,0) を現在回転で変換 → 機体の右方向（pitch 軸として使う）
   Quaternion currentRot    = transform->transform.GetActiveQuaternion();
   Vector3    localForward  = RotateVector({ 0.0f, 0.0f, 1.0f }, currentRot);
   Vector3    localRight    = RotateVector({ 1.0f, 0.0f, 0.0f }, currentRot);

   // 角速度を目標値に向けて更新する。
   // 入力がある場合は targetVel（= input * rollSpeed * deg2rad）に即座にセットする。
   // 入力がない場合は指数減衰（angularDamping）で 0 に戻す（慣性の表現）。
   UpdateAngularVelocity(-steerInput, angularVelYaw_,
						 -steerInput * rollSpeed * kDeg2Rad, deltaTime);
   UpdateAngularVelocity(pitchInput, angularVelPitch_,
						 pitchInput * pitchSpeed * kDeg2Rad, deltaTime);

   // yaw / pitch 回転をクォータニオンに合成する。
   Quaternion newRot = currentRot;
   ApplyRollRotation(newRot, localForward, deltaTime);
   ApplyPitchRotation(newRot, localRight, deltaTime);

   // 角速度がほぼゼロのときは transform への書き込みをスキップして
   // 浮動小数点の累積誤差による不要な更新を防ぐ。
   if (std::abs(angularVelYaw_) > 1e-6f || std::abs(angularVelPitch_) > 1e-6f) {
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
	  float   drag       = std::exp(-airDrag * deltaTime);
	  gravityBody->SetVelocity(horizontal * drag + vertical);
   }
}

// ---------------------------------------------------------------
// private
// ---------------------------------------------------------------

void VehicleAirController::UpdateAngularVelocity(float input, float& angVel,
												 float targetVel, float deltaTime) const {
   if (std::abs(input) > 1e-4f) {
	  // 入力がある場合: 角速度を即座に目標値（input × 角速度上限）にセットする。
	  // 慣性をつけるなら lerp にするが、即応性を優先してここでは即値代入にしている。
	  angVel = targetVel;
   } else {
	  // 入力がない場合: 指数減衰で角速度をゼロへ戻す。
	  // angVel *= exp(-angularDamping * dt) は時定数 1/angularDamping の
	  // 一階線形 ODE の厳密解であり、dt の大きさに関わらず安定している。
	  angVel *= std::exp(-angularDamping * deltaTime);
	  // 十分小さくなったらゼロにスナップして残留振動を防ぐ。
	  if (std::abs(angVel) < 0.001f) { angVel = 0.0f; }
   }
}

void VehicleAirController::ApplyYawRotation(Quaternion& rot,
											const Vector3& localUp, float deltaTime) const {
   if (std::abs(angularVelYaw_) <= 1e-6f) { return; }

   // localUp 軸まわりに angularVelYaw_ * dt ラジアン回転させるクォータニオンを作り、
   // 現在の回転に左から掛けて合成する（ワールド空間での回転軸を維持するため左掛け）。
   // 最後に Normalize して浮動小数点誤差の蓄積を防ぐ。
   Quaternion delta = MakeRotateAxisAngleQuaternion(localUp, angularVelYaw_ * deltaTime);
   rot = (delta * rot).Normalize();
}

void VehicleAirController::ApplyPitchRotation(Quaternion& rot,
											  const Vector3& localRight, float deltaTime) const {
   if (std::abs(angularVelPitch_) <= 1e-6f) { return; }

   // localRight 軸まわりの pitch 回転（前後傾き）を合成する。
   // yaw と同じく左掛けでワールド空間基準の軸を維持する。
   Quaternion delta = MakeRotateAxisAngleQuaternion(localRight, angularVelPitch_ * deltaTime);
   rot = (delta * rot).Normalize();
}

void VehicleAirController::ApplyRollRotation(GameEngine::Quaternion& rot, 
                                             const Vector3& localForward, float deltaTime) const {
   if (std::abs(angularVelYaw_) <= 1e-6f) { return; }

   // localForward 軸まわりの roll 回転（左右傾き）を合成する。
   Quaternion delta = MakeRotateAxisAngleQuaternion(localForward, angularVelYaw_ * deltaTime);
   rot = (delta * rot).Normalize();
}

// ---------------------------------------------------------------
// ImGui / Serialize
// ---------------------------------------------------------------

#ifdef USE_IMGUI
void VehicleAirController::DrawInspector() {
   if (!ImGui::CollapsingHeader("VehicleAirController")) { return; }
   ImGui::Separator();
   ImGui::DragFloat("Steer Speed##1",     &rollSpeed,     1.0f, 0.0f, 360.0f);
   ImGui::DragFloat("Pitch Speed",     &pitchSpeed,     1.0f, 0.0f, 360.0f);
   ImGui::DragFloat("Angular Damping", &angularDamping, 0.1f, 0.1f,  20.0f);
   ImGui::DragFloat("Air Drag",        &airDrag,        0.05f, 0.0f,  5.0f);
   ImGui::Spacing();
   ImGui::Text("AngVel Yaw/Pitch: %.2f / %.2f", angularVelYaw_, angularVelPitch_);
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
