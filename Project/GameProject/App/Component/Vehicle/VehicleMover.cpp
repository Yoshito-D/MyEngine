#include "VehicleMover.h"
#include "VehicleGroundMover.h"
#include "VehicleAirController.h"
#include "VehicleLandingAligner.h"
#include "VehicleLandingBoost.h"
#include "VehicleDrift.h"
#include "../Gravity/GravityBody.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include <cmath>
#include <numbers>
#include "Logger.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

using namespace GameEngine;

namespace App {

// ================================================================
// public
// ================================================================

void VehicleMover::ApplyMovement(float steerInput, float pitchInput, bool driftInput,
								 bool isGrounded,
								 const Vector3& gravityUp, float deltaTime) {
   auto* transform = GetOwner().GetComponent<TransformComponent>();
   if (!transform) { return; }

   // 現在フレームの回転クォータニオンを取得する。
   // 着地時の目標回転生成 (BuildAlignTargetRotation) と
   // 各サブコンポーネントへの転送のために必要。
   Quaternion currentRotation = transform->transform.GetActiveQuaternion();

   // ----------------------------------------------------------------
   // 着地遷移の検出
   // wasGrounded_ は前フレームの状態を保持している。
   // 「前フレームが空中 (false) かつ現フレームが接地 (true)」の瞬間が着地タイミング。
   // この瞬間に OnLanded() を呼んでブーストや姿勢補正を開始する。
   // ----------------------------------------------------------------
   if (isGrounded && !wasGrounded_) {
	  Logger::GameInfo("OnLanded");
	  OnLanded(currentRotation, gravityUp);
   }
   wasGrounded_ = isGrounded;

   // ----------------------------------------------------------------
   // 接地中 / 空中 の分岐
   // 同一フレームで両方を動かすことはない（着地した瞬間から接地処理を使う）。
   // ----------------------------------------------------------------
   if (isGrounded) {
	  auto* ground = GetOwner().GetComponent<VehicleGroundMover>();
	  if (ground) {
		 // ドリフト中はステアリング感度を driftSteerMult 倍にする。
		 // これにより、ドリフト中に向きの細かい調整がしやすくなる。
		 auto* drift = GetOwner().GetComponent<VehicleDrift>();
		 float effectiveSteer = steerInput;
		 if (drift && drift->IsDrifting()) {
			effectiveSteer = std::clamp(steerInput * drift->driftSteerMult, -1.0f, 1.0f);
		 }

		 ground->Apply(effectiveSteer, gravityUp, deltaTime);
		 // lastMoveDirection_ は VehicleController::GetLastMoveDirection() 経由で
		 // カメラ追従などに使われる。
		 lastMoveDirection_ = ground->GetFlatForward();
	  }

	  // ドリフトコンポーネントへステア・ドリフト入力を転送する。
	  // VehicleDrift は速度横滑りや状態管理を担当しており、
	  // 毎フレーム Apply() を呼ぶことで内部タイマーが正しく動作する。
	  auto* drift = GetOwner().GetComponent<VehicleDrift>();
	  if (drift) {
		 drift->Apply(driftInput, steerInput, gravityUp, deltaTime);
	  }
   } else {
	  // 空中では VehicleAirController が yaw/pitch 回転を担当する。
	  // ドリフトは接地中のみ有効なので空中では呼ばない。
	  auto* air = GetOwner().GetComponent<VehicleAirController>();
	  if (air) { air->Apply(steerInput, pitchInput, deltaTime); }
   }
}

// ================================================================
// private
// ================================================================

void VehicleMover::OnLanded(const Quaternion& currentRotation, const Vector3& gravityUp) {
   // 着地時の localUp を一度だけ計算してブースト判定と姿勢補正で共用する。
   // RotateVector は Y 軸 (0,1,0) を現在のクォータニオンで回した「車の上方向」を返す。
   Vector3 localUp = RotateVector({ 0.0f, 1.0f, 0.0f }, currentRotation);
   NotifyLandingBoost(localUp, gravityUp);
   ResetAirAngularVelocity();
   NotifyLandingAligner(currentRotation, gravityUp);
}

void VehicleMover::NotifyLandingBoost(const Vector3& localUp, const Vector3& gravityUp) {
   // VehicleLandingBoost は Optional コンポーネントなので
   // アタッチされていないとき (nullptr) は何もしない。
   auto* boost = GetOwner().GetComponent<VehicleLandingBoost>();
   if (boost) { boost->TryBoost(localUp, gravityUp); }
}

void VehicleMover::ResetAirAngularVelocity() {
   // 空中で蓄積した yaw/pitch 角速度を着地時にゼロに戻す。
   // リセットしないと着地後も慣性で回転し続けてしまう。
   auto* air = GetOwner().GetComponent<VehicleAirController>();
   if (air) { air->ResetAngularVelocity(); }
}

void VehicleMover::NotifyLandingAligner(const Quaternion& currentRotation,
										const Vector3& gravityUp) {
   auto* aligner = GetOwner().GetComponent<VehicleLandingAligner>();
   if (!aligner) { return; }

   // 目標回転（gravityUp に正立した姿勢）を生成してから
   // BeginAlign() で Slerp 補間を開始する。
   // 補間中は VehicleGroundMover が姿勢再構築をスキップするため
   // 二重に姿勢が書き換わることを防げる。
   Quaternion targetRot = BuildAlignTargetRotation(currentRotation, gravityUp);
   aligner->BeginAlign(currentRotation, targetRot);
}

Quaternion VehicleMover::BuildAlignTargetRotation(const Quaternion& currentRotation,
												  const Vector3& gravityUp) const {
   // ---- ステップ 1: 前方ベクトルを重力平面に投影する ----
   // 現在の回転から localForward（車の前方向）を取り出す。
   Vector3 localForward = RotateVector({ 0.0f, 0.0f, 1.0f }, currentRotation);

   // gravityUp とほぼ平行な場合（真上・真下から着地）は投影長がゼロになるため
   // フォールバックとして別方向を使ってゼロ除算を避ける。
   Vector3 fwdFlat = localForward - gravityUp * gravityUp.Dot(localForward);
   float   fwdLen  = fwdFlat.Length();
   if (fwdLen < 1e-4f) {
	  Vector3 tmp = (std::abs(gravityUp.x) < 0.9f) ? Vector3{1,0,0} : Vector3{0,0,1};
	  fwdFlat = tmp - gravityUp * gravityUp.Dot(tmp);
	  fwdLen  = fwdFlat.Length();
   }
   fwdFlat = fwdFlat * (1.0f / fwdLen);

   // ---- ステップ 2: 正規直交基底を構築する ----
   // right = gravityUp × fwdFlat（右方向）
   // fwdOut = right × gravityUp（再計算した前方。直交性を保証するため）
   Vector3 right  = gravityUp.Cross(fwdFlat).Normalize();
   Vector3 fwdOut = right.Cross(gravityUp).Normalize();

   // ---- ステップ 3: 回転行列 → クォータニオン変換（Shepperd の方法）----
   // 列順: [right | gravityUp | fwdOut]
   // トレースの正負と最大対角成分に応じて 4 つの場合分けをするのは、
   // sqrt の引数が負にならないよう数値的に安定した分岐を選ぶため。
   float m00=right.x, m10=right.y, m20=right.z;
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
   return q.Normalize();
}

// ================================================================
// ImGui / Serialize
// ================================================================

#ifdef USE_IMGUI
void VehicleMover::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) { return; }
   ImGui::Separator();
   ImGui::Text("%s", Tr("コーディネーター: サブコンポーネントへ処理を委譲します。", "Coordinator: delegates to sub-components."));
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("最後の移動方向", "Last Move Direction"),
	  lastMoveDirection_.x, lastMoveDirection_.y, lastMoveDirection_.z);
   ImGui::Text("%s: %s", Tr("接地していた", "Was Grounded"), wasGrounded_ ? Tr("はい", "yes") : Tr("いいえ", "no"));
}
#endif

nlohmann::json VehicleMover::Serialize() const {
   return nlohmann::json::object();
}

void VehicleMover::Deserialize(const nlohmann::json& data) {
   (void)data;
}

} // namespace App
