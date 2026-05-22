#include "VehicleGroundMover.h"
#include "../Gravity/GravityBody.h"
#include "VehicleLandingAligner.h"
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

void VehicleGroundMover::Apply(float steerInput, const Vector3& gravityUp, float deltaTime) {
   auto* transform = GetOwner().GetComponent<TransformComponent>();
   if (!transform) { return; }

   // ---- 速度更新 ----
   // autoSpeed へ向けて currentSpeed_ を毎フレーム近づける。
   // これにより着地ブーストや外部から加えられた速度変化が徐々に通常速度に戻る。
   UpdateSpeed(deltaTime);

   // ---- ステアリング ----
   // 現在のクォータニオンから前方ベクトルを取得し、
   // ステア入力に応じた yaw 回転を重力Up 軸まわりに加える。
   Quaternion currentRot   = transform->transform.GetActiveQuaternion();
   Vector3    localForward = RotateVector({ 0.0f, 0.0f, 1.0f }, currentRot);
   Vector3    steered      = ApplySteering(steerInput, localForward, gravityUp, deltaTime);

   // ---- 水平投影 ----
   // steered をそのまま使うと坂面や回転誤差で縦成分が混入することがある。
   // gravityUp 平面に投影して正規化することで純粋な「地面上の前進方向」を得る。
   flatForward_ = ProjectToHorizontalPlane(steered, gravityUp);

   // ---- GravityBody への速度書き込み ----
   // 水平速度を flatForward * currentSpeed_ に置き換える。
   // 垂直速度（重力加速や着地処理の結果）は保持するため Dot で取り出して加算する。
   ApplyVelocityToGravityBody(flatForward_, gravityUp);

   // ---- 姿勢再構築 ----
   // VehicleLandingAligner が Slerp 補正中の場合は姿勢を書き換えない。
   // 両方が同フレームに transform を書き換えると補正がキャンセルされるため。
   auto* aligner = GetOwner().GetComponent<VehicleLandingAligner>();
   if (aligner && aligner->IsAligning()) { return; }

   RebuildPosture(flatForward_, gravityUp);
}

// ---------------------------------------------------------------
// private
// ---------------------------------------------------------------

void VehicleGroundMover::UpdateSpeed(float deltaTime) {
   // currentSpeed_ が負値のとき（初回呼び出し）は即座に autoSpeed に初期化する。
   // 負値を「未初期化」フラグとして使うことで、シリアライズ値を上書きしない。
   if (currentSpeed_ < 0.0f) { currentSpeed_ = autoSpeed; }

   // 外部から積まれた瞬間速度変化（インパルス）を直接加算する。
   // ブースト・ペナルティなど「即時に速度を変えたい」場合に使う。
   currentSpeed_ += velocityImpulse_;
   velocityImpulse_ = 0.0f;

   // 外部から積まれた加速度を物理則どおりに適用する。
   // v += a * dt  (acceleration_ は適用後にリセット)
   currentSpeed_ += acceleration_ * deltaTime;
   acceleration_  = 0.0f;

   // 指数平滑（一次ローパスフィルタ）で autoSpeed に近づける。
   // speedRecovery が大きいほど速く収束する。
   // clamp(speedRecovery * dt, 0, 1) で補間率が 0〜1 に収まることを保証し、
   // dt が大きくても currentSpeed_ が autoSpeed を行き過ぎないようにする。
   currentSpeed_ += (autoSpeed - currentSpeed_) * std::clamp(speedRecovery * deltaTime, 0.0f, 1.0f);

   // maxSpeed を超えないように clamp する。これによりブーストやペナルティの極端な値も制限される。
   currentSpeed_ = std::clamp(currentSpeed_, -maxSpeed, maxSpeed);
}

void VehicleGroundMover::AddAcceleration(float accel) {
   // currentSpeed_ が未初期化（負値）のときは autoSpeed を初期値として使う。
   if (currentSpeed_ < 0.0f) { currentSpeed_ = autoSpeed; }
   // 加速度を積み上げる。実際に速度へ返換されるのは次の UpdateSpeed。
   acceleration_ += accel;
}

void VehicleGroundMover::AddVelocityImpulse(float impulse) {
   if (currentSpeed_ < 0.0f) { currentSpeed_ = autoSpeed; }
   // 速度に即座に加算する。autoSpeed への回復は UpdateSpeed の指数平滑に委ねる。
   velocityImpulse_ += impulse;
}

Vector3 VehicleGroundMover::ApplySteering(float steerInput, const Vector3& localForward,
										  const Vector3& gravityUp, float deltaTime) const {
   // 入力がほぼゼロなら回転不要なので現在の向きをそのまま返す。
   if (std::abs(steerInput) <= 1e-4f) { return localForward; }

   // ステアリング = 重力Up 軸まわりの yaw 回転。
   // steerSpeed (deg/sec) × deltaTime で今フレームの回転角(rad)を計算する。
   // kDeg2Rad: π/180、角度→ラジアン変換係数。
   constexpr float kDeg2Rad = static_cast<float>(std::numbers::pi) / 180.0f;
   float      yawAngle = steerInput * steerSpeed * kDeg2Rad * deltaTime;

   // MakeRotateAxisAngleQuaternion で yawAngle ラジアン分の yaw クォータニオンを作り、
   // 現在の前方ベクトルを回転させる。
   Quaternion yawDelta = MakeRotateAxisAngleQuaternion(gravityUp, yawAngle);
   return RotateVector(localForward, yawDelta);
}

Vector3 VehicleGroundMover::ProjectToHorizontalPlane(const Vector3& dir,
													 const Vector3& gravityUp) const {
   // dir から gravityUp 方向成分を除去する（グラム–シュミットの一段）。
   // flat = dir - (dir・gravityUp) * gravityUp
   // これにより flat は gravityUp と直交する水平面ベクトルになる。
   Vector3 flat    = dir - gravityUp * gravityUp.Dot(dir);
   float   flatLen = flat.Length();

   // dir が gravityUp とほぼ平行（真上・真下方向）の場合は flat がゼロベクトルになる。
   // そのままでは正規化で NaN になるため、別の軸で代替する。
   if (flatLen < 1e-4f) {
	  Vector3 tmp = (std::abs(gravityUp.x) < 0.9f) ? Vector3{ 1,0,0 } : Vector3{ 0,0,1 };
	  flat    = tmp - gravityUp * gravityUp.Dot(tmp);
	  flatLen = flat.Length();
   }
   return flat * (1.0f / flatLen);
}

void VehicleGroundMover::ApplyVelocityToGravityBody(const Vector3& flatForward,
													const Vector3& gravityUp) {
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   if (!gravityBody) { return; }

   // 既存の速度から重力方向成分（垂直速度）だけを取り出す。
   // Dot(gravityUp) で垂直方向の大きさを得て、gravityUp を掛けて垂直ベクトルを復元する。
   // これを足さないと、重力落下やジャンプの垂直成分が毎フレームリセットされてしまう。
   float   verticalSpeed = gravityBody->GetVelocity().Dot(gravityUp);
   gravityBody->SetVelocity(flatForward * currentSpeed_ + gravityUp * verticalSpeed);
}

void VehicleGroundMover::RebuildPosture(const Vector3& flatForward, const Vector3& gravityUp) {
   auto* transform = GetOwner().GetComponent<TransformComponent>();
   if (!transform) { return; }

   // 正規直交基底を構築する。
   // right  = gravityUp × flatForward（右方向）
   // fwdOut = right × gravityUp（再直交化した前方。直交性を完全に保証するため再計算する）
   // この手順により、(right, gravityUp, fwdOut) が完全な右手系直交基底になる。
   Vector3 right  = gravityUp.Cross(flatForward).Normalize();
   Vector3 fwdOut = right.Cross(gravityUp).Normalize();

   // 基底からクォータニオンを生成して transform に書き込む。
   transform->transform.SetRotationQuaternion(BasisToQuaternion(right, gravityUp, fwdOut));

   // GravityBody にも現在の Up 方向を通知する。
   // GravityBody は「重力から見た現在の上向き」を内部で持っており、
   // これを更新しないと回転補正が古い Up に向かって動き続ける。
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   if (gravityBody) {
	  gravityBody->SetCurrentUpVector(gravityUp);
	  gravityBody->SetTargetUpVector(gravityUp);
   }
}

Quaternion VehicleGroundMover::BasisToQuaternion(const Vector3& right,
												 const Vector3& up,
												 const Vector3& fwd) {
   // 3×3 回転行列の要素を列ベクトルとして並べる。
   // [right | up | fwd] の順が OpenGL 列順（右手系）。
   //   列0（X 軸）= right
   //   列1（Y 軸）= up
   //   列2（Z 軸）= fwd
   float m00=right.x, m10=right.y, m20=right.z;
   float m01=up.x,    m11=up.y,    m21=up.z;
   float m02=fwd.x,   m12=fwd.y,   m22=fwd.z;

   // Shepperd 法による回転行列→クォータニオン変換。
   // trace（対角和）= 4w² - 1 が正のときは w が最大成分なので標準式を使う。
   // trace が負のときは w が小さく精度が落ちるため、最大の対角成分を基準に分岐する。
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

// ---------------------------------------------------------------
// ImGui / Serialize
// ---------------------------------------------------------------

#ifdef USE_IMGUI
void VehicleGroundMover::DrawInspector() {
   if (!ImGui::CollapsingHeader("VehicleGroundMover")) { return; }
   ImGui::Separator();
   ImGui::DragFloat("Auto Speed",     &autoSpeed,     0.1f, 0.0f, 100.0f);
   ImGui::DragFloat("Steer Speed",    &steerSpeed,    1.0f, 0.0f, 360.0f);
   ImGui::DragFloat("Speed Recovery", &speedRecovery, 0.1f, 0.1f,  20.0f);
   ImGui::Spacing();
   ImGui::Text("CurrentSpeed: %.2f", currentSpeed_);
   ImGui::Text("FlatForward: (%.2f, %.2f, %.2f)", flatForward_.x, flatForward_.y, flatForward_.z);
}
#endif

nlohmann::json VehicleGroundMover::Serialize() const {
   nlohmann::json json;
   json["autoSpeed"]     = autoSpeed;
   json["steerSpeed"]    = steerSpeed;
   json["speedRecovery"] = speedRecovery;
   return json;
}

void VehicleGroundMover::Deserialize(const nlohmann::json& data) {
   if (data.contains("autoSpeed"))     { autoSpeed     = data["autoSpeed"]; }
   if (data.contains("steerSpeed"))    { steerSpeed     = data["steerSpeed"]; }
   if (data.contains("speedRecovery")) { speedRecovery  = data["speedRecovery"]; }
}

} // namespace App
