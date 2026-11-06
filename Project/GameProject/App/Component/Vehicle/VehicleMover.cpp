#include "GameProject/App/Component/Vehicle/VehicleMover.h"
#include "GameEngine/Math/Functions/MathConstants.h"
#include "GameProject/App/Component/Gravity/PlanetSwitcher.h"
#include "GameProject/App/Component/Vehicle/VehicleGroundMover.h"
#include "GameProject/App/Component/Vehicle/VehicleAirController.h"
#include "GameProject/App/Component/Vehicle/VehicleLandingAligner.h"
#include "GameProject/App/Component/Vehicle/VehicleLandingBoost.h"
#include "GameProject/App/Component/Vehicle/VehicleDrift.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include <cmath>
#include <numbers>
#include "GameEngine/Utility/Logger.h"

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

using namespace GameEngine;

namespace App {

// ================================================================
// 公開設定
// ================================================================

Quaternion VehicleMover::AlignToGravity(const Quaternion& rotation, const Vector3& gravityUp, float deltaTime) {

   const auto* switcher = GetOwner().GetComponent<PlanetSwitcher>();
   if (switcher && switcher->HasPendingPlanetChange()) {
      return rotation;
   }

   // 現在Upと目標Upを正規化
   Vector3 current = orientationUp_.Normalize();
   Vector3 target = gravityUp.Normalize();

   if (current.LengthSquared() < 1e-8f) {
      current = Vector3{0.0f, 1.0f, 0.0f};
      orientationUp_ = current;
   }

   if (target.LengthSquared() < 1e-8f) {
      target = Vector3{0.0f, 1.0f, 0.0f};
   }

   // 方向差が小さい場合は補間不要
   float dot = current.Dot(target);

   if (dot > 0.9999f) {

      return rotation;
   }

   // ほぼ逆向きの場合は180度回転の特別処理
   if (dot < -0.9999f) {
      Vector3 axis = Vector3{1.0f, 0.0f, 0.0f}.Cross(current);

      if (axis.LengthSquared() < 1e-6f) {
         axis = Vector3{0.0f, 1.0f, 0.0f}.Cross(current);
      }

      if (axis.LengthSquared() < 1e-6f) {
         axis = Vector3{0.0f, 0.0f, 1.0f}.Cross(current);
      }

      if (axis.LengthSquared() < 1e-6f) {

         return rotation;
      }

      axis = axis.Normalize();
      Quaternion rotDelta = MakeRotateAxisAngleQuaternion(axis, MathConstants::kPi);
      Quaternion cur = rotation;
      orientationUp_ = target;
      return (rotDelta * cur).Normalize();
   }

   // 回転軸を求める
   Vector3 rotAxis = current.Cross(target);

   if (rotAxis.LengthSquared() < 1e-6f) {

      return rotation;
   }

   rotAxis = rotAxis.Normalize();

   // 角度差を算出
   float angle = std::acos(std::clamp(dot, -1.0f, 1.0f));

   if (std::abs(angle) < 1e-6f) {

      return rotation;
   }

   // gravityAlignmentSpeed_ に基づき段階的に回転
   float t = std::clamp(gravityAlignmentSpeed_ * deltaTime, 0.0f, 1.0f);
   Quaternion rotDelta = MakeRotateAxisAngleQuaternion(rotAxis, angle * t);
   Quaternion cur = rotation;

   // 現在Upも補間更新
   if (t >= 0.9999f) {
      orientationUp_ = target;
   } else {
      orientationUp_ = Vector3::Lerp(current, target, t);
   }
   return (rotDelta * cur).Normalize();
}

void VehicleMover::ApplyMovement(float steerInput, float rollInput, float pitchInput, bool driftInput, bool isGrounded,
   const Vector3& gravityUp, float deltaTime) {
   if (!HasOwner() || !IsEnabled()) return;

   auto* transform = GetOwner().GetComponent<TransformComponent>();

   if (!transform) return;

   // 着地で開始するタイマーを同フレームに消費せず、地上とドリフトで同じ禁止状態を使う。
   // 空中でも時間を進め、ジャンプによって禁止時間が延びないようにする。
   auto* groundMover = GetOwner().GetComponent<VehicleGroundMover>();

   if (groundMover && groundMover->IsEnabled()) {
      groundMover->AdvanceAccelerationBlock(deltaTime);
   }

   // 現在フレームの回転クォータニオンを取得する。
   // 着地時の目標回転生成 (BuildAlignTargetRotation) と
   // 各サブコンポーネントへの転送のために必要。
   Quaternion currentRotation = transform->GetLocalPose().GetActiveQuaternion();

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
   Quaternion rotation = AlignToGravity(currentRotation, gravityUp, deltaTime);

   // ----------------------------------------------------------------
   // 接地中 / 空中 の分岐
   // 同一フレームで両方を動かすことはない（着地した瞬間から接地処理を使う）。
   // ----------------------------------------------------------------
   if (isGrounded) {
      auto* ground = GetOwner().GetComponent<VehicleGroundMover>();

      if (ground && ground->IsEnabled()) {
         // ドリフト中はステアリング感度を driftSteerMult 倍にする。
         // これにより、ドリフト中に向きの細かい調整がしやすくなる。
         auto* drift = GetOwner().GetComponent<VehicleDrift>();
         const float effectiveSteer =
            drift && drift->IsEnabled() ? drift->CalculateSteeringInput(steerInput) : steerInput;

         rotation = ground->Apply(effectiveSteer, gravityUp, deltaTime, rotation);

         // lastMoveDirection_ は VehicleController::GetLastMoveDirection() 経由で
         // カメラ追従などに使われる。
         lastMoveDirection_ = ground->GetFlatForward();
      }

      // ドリフトコンポーネントへステア・ドリフト入力を転送する。
      // VehicleDrift は速度横滑りや状態管理を担当しており、
      // 毎フレーム Apply() を呼ぶことで内部タイマーが正しく動作する。
      auto* drift = GetOwner().GetComponent<VehicleDrift>();

      if (drift && drift->IsEnabled()) {
         drift->Apply(driftInput, steerInput, gravityUp, deltaTime);
      }
   } else {
      // 空中では地上操舵を使わず、専用の roll/pitch 入力だけを姿勢へ反映する。
      // ドリフトは接地中のみ有効なので空中では呼ばない。
      auto* air = GetOwner().GetComponent<VehicleAirController>();

      if (air && air->IsEnabled()) {
         rotation = air->Apply(rollInput, pitchInput, deltaTime, rotation);
      }
   }
   if (isGrounded) {
      auto* aligner = GetOwner().GetComponent<VehicleLandingAligner>();
      if (aligner && aligner->IsEnabled()) {
         aligner->Evaluate(deltaTime, rotation);
      }
      orientationUp_ = RotateVector({0.0f, 1.0f, 0.0f}, rotation);
   }

   // 地上操舵・空中入力・着地補間を選択した後、車体姿勢はここで一度だけ適用する。
   auto pose = transform->GetLocalPose();
   pose.SetRotationQuaternion(rotation);
   transform->ApplyLocalPose(pose);
}

// ================================================================
// 内部設定
// ================================================================

void VehicleMover::OnLanded(const Quaternion& currentRotation, const Vector3& gravityUp) {
   // 着地時の localUp を一度だけ計算してブースト判定と姿勢補正で共用する。
   // RotateVector は Y 軸 (0,1,0) を現在のクォータニオンで回した「車の上方向」を返す。
   Vector3 localUp = RotateVector({0.0f, 1.0f, 0.0f}, currentRotation);
   NotifyLandingBoost(localUp, gravityUp);
   ResetAirAngularVelocity();
   NotifyLandingAligner(currentRotation, gravityUp);
}

void VehicleMover::NotifyLandingBoost(const Vector3& localUp, const Vector3& gravityUp) {
   // VehicleLandingBoost は Optional コンポーネントなので
   // アタッチされていないとき (nullptr) は何もしない。
   auto* boost = GetOwner().GetComponent<VehicleLandingBoost>();

   if (boost && boost->IsEnabled()) {
      boost->TryBoost(localUp, gravityUp);
   }
}

void VehicleMover::ResetAirAngularVelocity() {
   // 空中で蓄積した roll/pitch 角速度を着地時にゼロに戻す。
   // リセットしないと着地後も慣性で回転し続けてしまう。
   auto* air = GetOwner().GetComponent<VehicleAirController>();

   if (air && air->IsEnabled()) {
      air->ResetAngularVelocity();
   }
}

void VehicleMover::NotifyLandingAligner(const Quaternion& currentRotation, const Vector3& gravityUp) {
   auto* aligner = GetOwner().GetComponent<VehicleLandingAligner>();

   if (!aligner || !aligner->IsEnabled()) return;

   // 目標回転（gravityUp に正立した姿勢）を生成してから
   // BeginAlign() で Slerp 補間を開始する。
   // 最終姿勢の選択時に、地上操舵の候補より着地補間の候補を優先する。
   Quaternion targetRot = BuildAlignTargetRotation(currentRotation, gravityUp);
   aligner->BeginAlign(currentRotation, targetRot);
}

Quaternion VehicleMover::BuildAlignTargetRotation(const Quaternion& currentRotation, const Vector3& gravityUp) const {
   // ---- ステップ 1: 前方ベクトルを重力平面に投影する ----
   // 現在の回転から localForward（車の前方向）を取り出す。
   Vector3 localForward = RotateVector({0.0f, 0.0f, 1.0f}, currentRotation);

   // gravityUp とほぼ平行な場合（真上・真下から着地）は投影長がゼロになるため
   // フォールバックとして別方向を使ってゼロ除算を避ける。
   Vector3 fwdFlat = localForward - gravityUp * gravityUp.Dot(localForward);
   float fwdLen = fwdFlat.Length();

   if (fwdLen < 1e-4f) {
      Vector3 tmp = (std::abs(gravityUp.x) < 0.9f) ? Vector3{1, 0, 0} : Vector3{0, 0, 1};
      fwdFlat = tmp - gravityUp * gravityUp.Dot(tmp);
      fwdLen = fwdFlat.Length();
   }

   fwdFlat = fwdFlat * (1.0f / fwdLen);

   // 共通の基底変換を使い、地上姿勢と着地姿勢の計算規則を揃える。
   return LookRotation(fwdFlat, gravityUp);
}

// ================================================================
// ImGui / シリアライズ
// ================================================================

#ifdef USE_IMGUI
void VehicleMover::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!ImGui::CollapsingHeader(header.c_str())) return;

   ImGui::Separator();
   ImGui::DragFloat(
      Tr("重力姿勢追従速度 (1/s)", "Gravity Alignment Speed (1/s)"), &gravityAlignmentSpeed_, 0.1f, 0.0f, 100.0f);
   ImGui::Text("%s",
      Tr("コーディネーター: サブコンポーネントへ処理を委譲します。", "Coordinator: delegates to sub-components."));
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("最後の移動方向", "Last Move Direction"), lastMoveDirection_.x,
      lastMoveDirection_.y, lastMoveDirection_.z);
   ImGui::Text("%s: %s", Tr("接地していた", "Was Grounded"), wasGrounded_ ? Tr("はい", "yes") : Tr("いいえ", "no"));
}
#endif

nlohmann::json VehicleMover::GetSharedParameters() const {
   return Serialize();
}

void VehicleMover::ApplySharedParameters(const nlohmann::json& data) {
   Deserialize(data);
}

nlohmann::json VehicleMover::Serialize() const {
   return nlohmann::json{{"gravityAlignmentSpeed", gravityAlignmentSpeed_}};
}

void VehicleMover::Deserialize(const nlohmann::json& data) {
   const float speed = data.contains("gravityAlignmentSpeed") && data.at("gravityAlignmentSpeed").is_number()
                          ? data.at("gravityAlignmentSpeed").get<float>()
                          : gravityAlignmentSpeed_;
   gravityAlignmentSpeed_ = std::isfinite(speed) ? std::max(0.0f, speed) : 5.0f;
}

} // namespace App
