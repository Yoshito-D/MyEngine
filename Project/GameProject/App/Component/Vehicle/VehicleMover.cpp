#include "VehicleMover.h"
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

void VehicleMover::ApplyMovement(float steerInput, float pitchInput, bool isGrounded,
								  const Vector3& gravityUp, float deltaTime) {
   auto* transform = GetOwner().GetComponent<TransformComponent>();
   if (!transform) { return; }

   // 現在の回転を取得
   Quaternion currentRotation = transform->transform.GetActiveQuaternion();

   // 現在の車体 Up・前方・右方向を計算
   Vector3 localUp      = RotateVector({ 0.0f, 1.0f, 0.0f }, currentRotation);
   Vector3 localForward = RotateVector({ 0.0f, 0.0f, 1.0f }, currentRotation);
   Vector3 localRight   = RotateVector({ 1.0f, 0.0f, 0.0f }, currentRotation);

   constexpr float kDeg2Rad = static_cast<float>(std::numbers::pi) / 180.0f;

   auto* gravityBody = GetOwner().GetComponent<GravityBody>();

   // --- 着地遷移検出 ---
   if (isGrounded && !wasGrounded_) {
	  angularVelYaw_   = 0.0f;
	  angularVelPitch_ = 0.0f;

	  // 着地した瞬間の並行度を判定して初期速度を決定する
	  float alignment = std::clamp(localUp.Dot(gravityUp), 0.0f, 1.0f);
	  currentSpeed_ = (alignment >= boostThreshold)
					  ? autoSpeed + boostAmount
					  : autoSpeed;

	  // Slerp 補正用: 開始クォータニオンを記録
	  alignStartRotation_ = currentRotation;

	  // 目標クォータニオン: gravityUp を上方向としつつ、現在の前方を維持した姿勢を構築
	  // 現在 forward を gravityUp 平面に投影して正規化
	  Vector3 fwdFlat = localForward - gravityUp * gravityUp.Dot(localForward);
	  float   fwdLen  = fwdFlat.Length();
	  if (fwdLen < 1e-4f) {
		 // forward が gravityUp に平行な場合は別軸から生成
		 Vector3 tmp = (std::abs(gravityUp.x) < 0.9f) ? Vector3{ 1,0,0 } : Vector3{ 0,0,1 };
		 fwdFlat = tmp - gravityUp * gravityUp.Dot(tmp);
		 fwdLen  = fwdFlat.Length();
	  }
	  fwdFlat = fwdFlat * (1.0f / fwdLen);
	  Vector3 right  = gravityUp.Cross(fwdFlat).Normalize();
	  Vector3 fwdOut = right.Cross(gravityUp).Normalize();

	  // 回転行列からクォータニオンへ変換（row-basis: x=right, y=up, z=fwd）
	  float m00=right.x,  m10=right.y,  m20=right.z;
	  float m01=gravityUp.x, m11=gravityUp.y, m21=gravityUp.z;
	  float m02=fwdOut.x, m12=fwdOut.y, m22=fwdOut.z;
	  float trace = m00 + m11 + m22;
	  Quaternion tgt;
	  if (trace > 0.0f) {
		 float s = 0.5f / std::sqrt(trace + 1.0f);
		 tgt.w = 0.25f / s; tgt.x = (m21-m12)*s; tgt.y = (m02-m20)*s; tgt.z = (m10-m01)*s;
	  } else if (m00 > m11 && m00 > m22) {
		 float s = 2.0f * std::sqrt(1.0f + m00 - m11 - m22);
		 tgt.w = (m21-m12)/s; tgt.x = 0.25f*s; tgt.y = (m01+m10)/s; tgt.z = (m02+m20)/s;
	  } else if (m11 > m22) {
		 float s = 2.0f * std::sqrt(1.0f + m11 - m00 - m22);
		 tgt.w = (m02-m20)/s; tgt.x = (m01+m10)/s; tgt.y = 0.25f*s; tgt.z = (m12+m21)/s;
	  } else {
		 float s = 2.0f * std::sqrt(1.0f + m22 - m00 - m11);
		 tgt.w = (m10-m01)/s; tgt.x = (m02+m20)/s; tgt.y = (m12+m21)/s; tgt.z = 0.25f*s;
	  }
	  alignTargetRotation_ = tgt.Normalize();

	  landingAlignTimer_ = landingAlignTime;
   }
   wasGrounded_ = isGrounded;

   if (isGrounded) {
	  // --- 接地中 ---

	  // ① 速度スケール更新
	  if (currentSpeed_ < 0.0f) { currentSpeed_ = autoSpeed; }
	  currentSpeed_ += (autoSpeed - currentSpeed_) * std::clamp(speedRecovery * deltaTime, 0.0f, 1.0f);

	  // ② ステアリング: yaw 入力があれば localForward を回転
	  if (std::abs(steerInput) > 1e-4f) {
		 float      yawAngle = steerInput * steerSpeed * kDeg2Rad * deltaTime;
		 Quaternion yawDelta = MakeRotateAxisAngleQuaternion(gravityUp, yawAngle);
		 localForward = RotateVector(localForward, yawDelta);
	  }

	  // ③ localForward を gravityUp 平面に投影して水平前方を確定
	  Vector3 flatForward = localForward - gravityUp * gravityUp.Dot(localForward);
	  {
		 float flatLen = flatForward.Length();
		 if (flatLen < 1e-4f) {
			Vector3 tmp = (std::abs(gravityUp.x) < 0.9f) ? Vector3{1,0,0} : Vector3{0,0,1};
			flatForward = tmp - gravityUp * gravityUp.Dot(tmp);
			flatLen     = flatForward.Length();
		 }
		 flatForward = flatForward * (1.0f / flatLen);
	  }

	  // ④ 速度を GravityBody に適用（姿勢再構築の前に行う）
	  if (gravityBody) {
		 Vector3 velocity      = gravityBody->GetVelocity();
		 float   verticalSpeed = velocity.Dot(gravityUp);
		 gravityBody->SetVelocity(flatForward * currentSpeed_ + gravityUp * verticalSpeed);
	  }
	  lastMoveDirection_ = flatForward;

	  // ⑤ 姿勢を最後に確定（速度適用後）
	  //    着地補正タイマー中は Slerp、それ以外は毎フレーム完全再構築
	  if (landingAlignTimer_ > 0.0f) {
		 landingAlignTimer_ -= deltaTime;
		 float      t       = 1.0f - std::clamp(landingAlignTimer_ / landingAlignTime, 0.0f, 1.0f);
		 Quaternion blended = Slerp(alignStartRotation_, alignTargetRotation_, t);

		 if (landingAlignTimer_ <= 0.0f) {
			landingAlignTimer_ = 0.0f;
			blended = alignTargetRotation_; // 時間終了時は完全スナップ
		 }
		 transform->transform.SetRotationQuaternion(blended);
		 if (gravityBody) {
			 Vector3 blendedUp = RotateVector({ 0.0f, 1.0f, 0.0f }, blended);
			 gravityBody->SetCurrentUpVector(blendedUp);
			 gravityBody->SetTargetUpVector(blendedUp);
		 }
	  } else {
		 // 通常接地: (gravityUp, flatForward) から姿勢を完全再構築
		 Vector3 right  = gravityUp.Cross(flatForward).Normalize();
		 Vector3 fwdOut = right.Cross(gravityUp).Normalize();

		 float m00=right.x,     m10=right.y,     m20=right.z;
		 float m01=gravityUp.x, m11=gravityUp.y, m21=gravityUp.z;
		 float m02=fwdOut.x,    m12=fwdOut.y,    m22=fwdOut.z;
		 float trace = m00 + m11 + m22;
		 Quaternion q;
		 if (trace > 0.0f) {
			float s = 0.5f / std::sqrt(trace + 1.0f);
			q.w=(0.25f/s); q.x=(m21-m12)*s; q.y=(m02-m20)*s; q.z=(m10-m01)*s;
		 } else if (m00 > m11 && m00 > m22) {
			float s = 2.0f * std::sqrt(1.0f + m00 - m11 - m22);
			q.w=(m21-m12)/s; q.x=0.25f*s; q.y=(m01+m10)/s; q.z=(m02+m20)/s;
		 } else if (m11 > m22) {
			float s = 2.0f * std::sqrt(1.0f + m11 - m00 - m22);
			q.w=(m02-m20)/s; q.x=(m01+m10)/s; q.y=0.25f*s; q.z=(m12+m21)/s;
		 } else {
			float s = 2.0f * std::sqrt(1.0f + m22 - m00 - m11);
			q.w=(m10-m01)/s; q.x=(m02+m20)/s; q.y=(m12+m21)/s; q.z=0.25f*s;
		 }
		 transform->transform.SetRotationQuaternion(q.Normalize());
		 if (gravityBody) {
			 gravityBody->SetCurrentUpVector(gravityUp);
			 gravityBody->SetTargetUpVector(gravityUp);
		 }
	  }

   } else {
	  // --- 空中中 ---
	  // 速度（位置）は一切変更しない

	  // ① 慣性角速度の更新
	  //    入力がある間はターゲット角速度へ向けて蓄積し、なくなると減衰
	  float targetYaw   = steerInput * steerSpeed * kDeg2Rad;
	  float targetPitch = pitchInput * pitchSpeed * kDeg2Rad;

	  if (std::abs(steerInput) > 1e-4f) {
		 // 入力方向へ即追従（慣性は「離したとき」に効く）
		 angularVelYaw_ = targetYaw;
	  } else {
		 // 入力なし → 減衰
		 angularVelYaw_ *= std::exp(-angularDamping * deltaTime);
		 if (std::abs(angularVelYaw_) < 0.001f) { angularVelYaw_ = 0.0f; }
	  }

	  if (std::abs(pitchInput) > 1e-4f) {
		 angularVelPitch_ = targetPitch;
	  } else {
		 angularVelPitch_ *= std::exp(-angularDamping * deltaTime);
		 if (std::abs(angularVelPitch_) < 0.001f) { angularVelPitch_ = 0.0f; }
	  }

	  // ② 角速度を実際の回転へ反映
	  Quaternion newRotation = currentRotation;

	  if (std::abs(angularVelYaw_) > 1e-6f) {
		 float      yawAngle = angularVelYaw_ * deltaTime;
		 Quaternion yawDelta = MakeRotateAxisAngleQuaternion(localUp, yawAngle);
		 newRotation = (yawDelta * newRotation).Normalize();
	  }

	  if (std::abs(angularVelPitch_) > 1e-6f) {
		 float      pitchAngle = angularVelPitch_ * deltaTime;
		 Quaternion pitchDelta = MakeRotateAxisAngleQuaternion(localRight, pitchAngle);
		 newRotation = (pitchDelta * newRotation).Normalize();
	  }

	  if (std::abs(angularVelYaw_) > 1e-6f || std::abs(angularVelPitch_) > 1e-6f) {
		 transform->transform.SetRotationQuaternion(newRotation);
	  }
   }
}

#ifdef USE_IMGUI
void VehicleMover::DrawInspector() {
   if (!ImGui::CollapsingHeader("VehicleMover")) { return; }
   ImGui::Separator();
   ImGui::DragFloat("Auto Speed",      &autoSpeed,         0.1f, 0.0f, 100.0f);
   ImGui::DragFloat("Steer Speed",     &steerSpeed,        1.0f, 0.0f, 360.0f);
   ImGui::DragFloat("Pitch Speed",     &pitchSpeed,        1.0f, 0.0f, 360.0f);
   ImGui::DragFloat("Boost Amount",    &boostAmount,       0.1f, 0.0f,  30.0f);
   ImGui::DragFloat("Boost Threshold", &boostThreshold,    0.01f, 0.0f,  1.0f);
   ImGui::DragFloat("Speed Recovery",  &speedRecovery,     0.1f, 0.1f,  20.0f);
   ImGui::DragFloat("Angular Damping", &angularDamping,    0.1f, 0.1f,  20.0f);
   ImGui::DragFloat("Land AlignTime",  &landingAlignTime,  0.01f, 0.05f, 2.0f);
   ImGui::Spacing();
   ImGui::Text("CurrentSpeed: %.2f  AlignTimer: %.2f", currentSpeed_, landingAlignTimer_);
   ImGui::Text("AngVel Yaw/Pitch: %.2f / %.2f", angularVelYaw_, angularVelPitch_);
   ImGui::Text("LastMoveDir: (%.2f, %.2f, %.2f)",
	  lastMoveDirection_.x, lastMoveDirection_.y, lastMoveDirection_.z);
}
#endif

nlohmann::json VehicleMover::Serialize() const {
   nlohmann::json json;
   json["autoSpeed"]         = autoSpeed;
   json["steerSpeed"]        = steerSpeed;
   json["pitchSpeed"]        = pitchSpeed;
   json["boostAmount"]       = boostAmount;
   json["boostThreshold"]    = boostThreshold;
   json["speedRecovery"]     = speedRecovery;
   json["angularDamping"]    = angularDamping;
   json["landingAlignTime"]  = landingAlignTime;
   return json;
}

void VehicleMover::Deserialize(const nlohmann::json& data) {
   if (data.contains("autoSpeed"))         { autoSpeed         = data["autoSpeed"]; }
   if (data.contains("steerSpeed"))        { steerSpeed        = data["steerSpeed"]; }
   if (data.contains("pitchSpeed"))        { pitchSpeed        = data["pitchSpeed"]; }
   if (data.contains("boostAmount"))       { boostAmount       = data["boostAmount"]; }
   if (data.contains("boostThreshold"))    { boostThreshold    = data["boostThreshold"]; }
   if (data.contains("speedRecovery"))     { speedRecovery     = data["speedRecovery"]; }
   if (data.contains("angularDamping"))    { angularDamping    = data["angularDamping"]; }
   if (data.contains("landingAlignTime"))  { landingAlignTime  = data["landingAlignTime"]; }
}

} // namespace App
