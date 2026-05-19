#include "VehicleLandingAligner.h"
#include "../Gravity/GravityBody.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

using namespace GameEngine;

namespace App {

// ---------------------------------------------------------------
// public
// ---------------------------------------------------------------

void VehicleLandingAligner::BeginAlign(const Quaternion& startRot,
									   const Quaternion& targetRot) {
   // 補正の開始姿勢・目標姿勢・残り時間を設定する。
   // alignTime 秒かけて startRot → targetRot へ Slerp 補間する。
   // 既に補正中の場合も現在地から再スタートするため、スムーズに繋がる。
   alignStart_  = startRot;
   alignTarget_ = targetRot;
   alignTimer_  = alignTime;
}

void VehicleLandingAligner::Update(float deltaTime) {
   // タイマーがゼロ以下なら補正は終了しているので何もしない。
   if (alignTimer_ <= 0.0f) { return; }

   auto* transform = GetOwner().GetComponent<TransformComponent>();
   if (!transform) { alignTimer_ = 0.0f; return; }

   // タイマーを減算する。負にならないようクランプしてから使う。
   alignTimer_ -= deltaTime;
   if (alignTimer_ <= 0.0f) { alignTimer_ = 0.0f; }

   // 補間比率を計算して姿勢を更新する。
   ApplyBlendedRotation(CalcBlendedRotation());
}

// ---------------------------------------------------------------
// private
// ---------------------------------------------------------------

Quaternion VehicleLandingAligner::CalcBlendedRotation() const {
   // タイマーが残り 0 なら目標姿勢をそのまま返す。
   if (alignTimer_ <= 0.0f) { return alignTarget_; }

   // t = 1 - (残り時間 / 総時間) で 0→1 に変化する補間パラメータを計算する。
   // 開始直後は t ≈ 0 (= startRot 側)、終了直前は t ≈ 1 (= targetRot 側)。
   // Slerp（球面線形補間）はクォータニオンの大円上を等角速度で補間するため、
   // 姿勢変化が一定速度で見える自然な補正になる。
   float t = 1.0f - std::clamp(alignTimer_ / alignTime, 0.0f, 1.0f);
   return Slerp(alignStart_, alignTarget_, t);
}

void VehicleLandingAligner::ApplyBlendedRotation(const Quaternion& blended) {
   auto* transform = GetOwner().GetComponent<TransformComponent>();
   if (!transform) { return; }

   // Transform に補間済み回転を書き込む。
   transform->transform.SetRotationQuaternion(blended);

   // GravityBody の Up ベクトルも補間値に合わせて更新する。
   // GravityBody は「現在の Up」と「目標の Up」を両方持っており、
   // これを更新しないと重力補正が古い Up に向かって動き続けて
   // ランディングアライナーの補正と干渉してしまう。
   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   if (gravityBody) {
	  Vector3 blendedUp = RotateVector({ 0.0f, 1.0f, 0.0f }, blended);
	  gravityBody->SetCurrentUpVector(blendedUp);
	  gravityBody->SetTargetUpVector(blendedUp);
   }
}

// ---------------------------------------------------------------
// ImGui / Serialize
// ---------------------------------------------------------------

#ifdef USE_IMGUI
void VehicleLandingAligner::DrawInspector() {
   if (!ImGui::CollapsingHeader("VehicleLandingAligner")) { return; }
   ImGui::Separator();
   ImGui::DragFloat("Align Time", &alignTime, 0.01f, 0.05f, 2.0f);
   ImGui::Spacing();
   ImGui::Text("AlignTimer: %.2f  Aligning: %s", alignTimer_, IsAligning() ? "yes" : "no");
}
#endif

nlohmann::json VehicleLandingAligner::Serialize() const {
   nlohmann::json json;
   json["alignTime"] = alignTime;
   return json;
}

void VehicleLandingAligner::Deserialize(const nlohmann::json& data) {
   if (data.contains("alignTime")) { alignTime = data["alignTime"]; }
}

} // namespace App
