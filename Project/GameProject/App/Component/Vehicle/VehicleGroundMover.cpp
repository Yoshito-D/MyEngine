#include "GameProject/App/Component/Vehicle/VehicleGroundMover.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Math/Functions/MathConstants.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include <cmath>

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

using namespace GameEngine;

namespace App {

// ---------------------------------------------------------------
// 公開設定
// ---------------------------------------------------------------

Quaternion VehicleGroundMover::Apply(
   float steerInput, const Vector3& gravityUp, float deltaTime, const Quaternion& currentRot) {
   if (!IsEnabled() || !HasOwner()) {
      return currentRot;
   }

   // ---- 速度更新 ----
   // autoSpeed へ向けて currentSpeed_ を毎フレーム近づける。
   // これにより着地ブーストや外部から加えられた速度変化が徐々に通常速度に戻る。
   UpdateSpeed(deltaTime);

   // ---- ステアリング ----
   // 現在のクォータニオンから前方ベクトルを取得し、
   // ステア入力に応じた yaw 回転を重力Up 軸まわりに加える。
   Vector3 localForward = RotateVector({0.0f, 0.0f, 1.0f}, currentRot);
   Vector3 steered = ApplySteering(steerInput, localForward, gravityUp, deltaTime);

   // ---- 水平投影 ----
   // steered をそのまま使うと坂面や回転誤差で縦成分が混入することがある。
   // gravityUp 平面に投影して正規化することで純粋な「地面上の前進方向」を得る。
   flatForward_ = ProjectToHorizontalPlane(steered, gravityUp);

   // ---- GravityBody への速度書き込み ----
   // 水平速度を flatForward * currentSpeed_ に置き換える。
   // 垂直速度（重力加速や着地処理の結果）は保持するため Dot で取り出して加算する。
   ApplyVelocityToGravityBody(flatForward_, gravityUp);

   // 最終姿勢の選択とTransformへの適用は、移動の調整役に集約する。
   return LookRotation(flatForward_, gravityUp);
}

// ---------------------------------------------------------------
// 内部設定
// ---------------------------------------------------------------

void VehicleGroundMover::UpdateSpeed(float deltaTime) {
   // currentSpeed_ が負値のとき（初回呼び出し）は即座に autoSpeed に初期化する。
   // 負値を「未初期化」フラグとして使うことで、シリアライズ値を上書きしない。
   if (currentSpeed_ < 0.0f) {
      currentSpeed_ = kInitialSpeed;
   }

   // 外部から積まれた瞬間速度変化（インパルス）を直接加算する。
   // ブースト・ペナルティなど「即時に速度を変えたい」場合に使う。
   currentSpeed_ += velocityImpulse_;
   velocityImpulse_ = 0.0f;

   // 外部から積まれた加速度を物理則どおりに適用する。
   // v += a * dt  (acceleration_ は適用後にリセット)
   currentSpeed_ += acceleration_ * deltaTime;
   acceleration_ = 0.0f;

   // 指数平滑（一次ローパスフィルタ）で autoSpeed に近づける。
   // speedRecovery が大きいほど速く収束する。
   // clamp(speedRecovery * dt, 0, 1) で補間率が 0〜1 に収まることを保証し、
   // dt が大きくても currentSpeed_ が autoSpeed を行き過ぎないようにする。
   // 禁止中も通常速度を超えた分の減衰は許可するが、速度回復は行わない。
   if (!IsAccelerationBlocked() || currentSpeed_ > autoSpeed) {
      currentSpeed_ += (autoSpeed - currentSpeed_) * std::clamp(speedRecovery * deltaTime, 0.0f, 1.0f);
   }

   // maxSpeed を超えないように clamp する。これによりブーストやペナルティの極端な値も制限される。
   currentSpeed_ = std::clamp(currentSpeed_, -maxSpeed, maxSpeed);
}

void VehicleGroundMover::ApplyLandingPenalty(float speed, float duration) {
   // 減速と加速禁止を同じ操作にし、着地直前の保留中ブーストも確実に破棄する。
   currentSpeed_ = speed;
   BlockAcceleration(duration);
}

void VehicleGroundMover::BlockAcceleration(float seconds) {
   accelerationBlockRemaining_ = std::max(accelerationBlockRemaining_, std::max(0.0f, seconds));

   if (!IsAccelerationBlocked()) return;

   // 着地直前のミニターボなどが次フレームに持ち越されないようにする。
   velocityImpulse_ = std::min(0.0f, velocityImpulse_);
   acceleration_ = std::min(0.0f, acceleration_);
}

void VehicleGroundMover::AdvanceAccelerationBlock(float deltaTime) {
   accelerationBlockRemaining_ = std::max(0.0f, accelerationBlockRemaining_ - std::max(0.0f, deltaTime));
}

void VehicleGroundMover::AddAcceleration(float accel) {
   if (IsAccelerationBlocked() && accel > 0.0f) return;

   // currentSpeed_ が未初期化（負値）のときは autoSpeed を初期値として使う。
   if (currentSpeed_ < 0.0f) {
      currentSpeed_ = autoSpeed;
   }

   // 加速度を積み上げる。実際に速度へ返換されるのは次の UpdateSpeed。
   acceleration_ += accel;
}

void VehicleGroundMover::AddVelocityImpulse(float impulse) {
   if (IsAccelerationBlocked() && impulse > 0.0f) return;

   if (currentSpeed_ < 0.0f) {
      currentSpeed_ = autoSpeed;
   }

   // 速度に即座に加算する。autoSpeed への回復は UpdateSpeed の指数平滑に委ねる。
   velocityImpulse_ += impulse;
}

Vector3 VehicleGroundMover::ApplySteering(
   float steerInput, const Vector3& localForward, const Vector3& gravityUp, float deltaTime) const {
   // 入力がほぼゼロなら回転不要なので現在の向きをそのまま返す。
   if (std::abs(steerInput) <= 1e-4f) {
      return localForward;
   }

   // ステアリング = 重力Up 軸まわりの yaw 回転。
   // steerSpeed (deg/sec) × deltaTime で今フレームの回転角(rad)を計算する。
   float yawAngle = steerInput * steerSpeed * MathConstants::kDegreesToRadians * deltaTime;

   // MakeRotateAxisAngleQuaternion で yawAngle ラジアン分の yaw クォータニオンを作り、
   // 現在の前方ベクトルを回転させる。
   Quaternion yawDelta = MakeRotateAxisAngleQuaternion(gravityUp, yawAngle);
   return RotateVector(localForward, yawDelta);
}

Vector3 VehicleGroundMover::ProjectToHorizontalPlane(const Vector3& dir, const Vector3& gravityUp) const {
   // dir から gravityUp 方向成分を除去する（グラム–シュミットの一段）。
   // flat = dir - (dir・gravityUp) * gravityUp
   // これにより flat は gravityUp と直交する水平面ベクトルになる。
   Vector3 flat = dir - gravityUp * gravityUp.Dot(dir);
   float flatLen = flat.Length();

   // dir が gravityUp とほぼ平行（真上・真下方向）の場合は flat がゼロベクトルになる。
   // そのままでは正規化で NaN になるため、別の軸で代替する。
   if (flatLen < 1e-4f) {
      Vector3 tmp = (std::abs(gravityUp.x) < 0.9f) ? Vector3{1, 0, 0} : Vector3{0, 0, 1};
      flat = tmp - gravityUp * gravityUp.Dot(tmp);
      flatLen = flat.Length();
   }

   return flat * (1.0f / flatLen);
}

void VehicleGroundMover::ApplyVelocityToGravityBody(const Vector3& flatForward, const Vector3& gravityUp) {
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();

   if (!gravityBody || !gravityBody->IsEnabled()) return;

   // 既存の速度から重力方向成分（垂直速度）だけを取り出す。
   // Dot(gravityUp) で垂直方向の大きさを得て、gravityUp を掛けて垂直ベクトルを復元する。
   // これを足さないと、重力落下やジャンプの垂直成分が毎フレームリセットされてしまう。
   gravityBody->ApplySurfaceVelocity(flatForward * currentSpeed_, gravityUp);
}

// ---------------------------------------------------------------
// ImGui / シリアライズ
// ---------------------------------------------------------------

#ifdef USE_IMGUI
void VehicleGroundMover::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str())) return;

   ImGui::Separator();
   ImGui::DragFloat(Tr("自動速度", "Auto Speed"), &autoSpeed, 0.1f, 0.0f, 100.0f);
   ImGui::DragFloat((std::string(Tr("操舵速度", "Steer Speed")) + "##2").c_str(), &steerSpeed, 1.0f, 0.0f, 360.0f);
   ImGui::DragFloat(Tr("速度回復", "Speed Recovery"), &speedRecovery, 0.1f, 0.1f, 20.0f);
   ImGui::DragFloat(Tr("最大速度", "Max Speed"), &maxSpeed, 0.1f, 0.0f, 200.0f);
   ImGui::Spacing();

   if (ImGui::Button(Tr("速度リセット", "Reset Speed"))) {
      currentSpeed_ = 0.0f;
   }

   ImGui::Spacing();
   ImGui::Text("%s: %.2f", Tr("現在速度", "Current Speed"), currentSpeed_);
   ImGui::Text(
      "%s: (%.2f, %.2f, %.2f)", Tr("水平前方向", "Flat Forward"), flatForward_.x, flatForward_.y, flatForward_.z);
   Configure(DescribeSettings());
}
#endif

nlohmann::json VehicleGroundMover::Serialize() const {
   nlohmann::json json;
   json["autoSpeed"] = autoSpeed;
   json["steerSpeed"] = steerSpeed;
   json["speedRecovery"] = speedRecovery;
   json["maxSpeed"] = maxSpeed;
   return json;
}

nlohmann::json VehicleGroundMover::GetSharedParameters() const {
   return ExtractSharedParameters({});
}

void VehicleGroundMover::Deserialize(const nlohmann::json& data) {
   ReadParameters(data, true);
}

void VehicleGroundMover::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data, false);
}

void VehicleGroundMover::ReadParameters(const nlohmann::json& data, [[maybe_unused]] bool initializeRuntime) {
   if (data.contains("autoSpeed")) {
      autoSpeed = data["autoSpeed"];
   }

   if (data.contains("steerSpeed")) {
      steerSpeed = data["steerSpeed"];
   }

   if (data.contains("speedRecovery")) {
      speedRecovery = data["speedRecovery"];
   }

   if (data.contains("maxSpeed")) {
      maxSpeed = data["maxSpeed"];
   }

   Configure(DescribeSettings());
}

} // namespace App

namespace App {

VehicleGroundMover::Settings VehicleGroundMover::DescribeSettings() const {
   Settings settings;
   settings.autoSpeed = autoSpeed;
   settings.steerSpeed = steerSpeed;
   settings.speedRecovery = speedRecovery;
   settings.maxSpeed = maxSpeed;
   return settings;
}

void VehicleGroundMover::Configure(const Settings& requested) {
   auto settings = requested;
   [[maybe_unused]] const Settings defaults;

   if (!std::isfinite(settings.autoSpeed)) {
      settings.autoSpeed = defaults.autoSpeed;
   }

   if (!std::isfinite(settings.steerSpeed)) {
      settings.steerSpeed = defaults.steerSpeed;
   }

   if (!std::isfinite(settings.speedRecovery)) {
      settings.speedRecovery = defaults.speedRecovery;
   }

   if (!std::isfinite(settings.maxSpeed)) {
      settings.maxSpeed = defaults.maxSpeed;
   }

   settings.maxSpeed = std::max(0.0f, settings.maxSpeed);
   settings.autoSpeed = std::clamp(settings.autoSpeed, 0.0f, settings.maxSpeed);
   settings.speedRecovery = std::max(0.0f, settings.speedRecovery);
   autoSpeed = settings.autoSpeed;
   steerSpeed = settings.steerSpeed;
   speedRecovery = settings.speedRecovery;
   maxSpeed = settings.maxSpeed;

   if (currentSpeed_ >= 0.0f) {
      currentSpeed_ = std::min(currentSpeed_, maxSpeed);
   }
}

} // namespace App
