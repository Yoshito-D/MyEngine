#include "CharacterWalker.h"
#include "../Camera/ScreenSpaceBasis.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include <cmath>
#include <algorithm>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

using namespace GameEngine;

namespace App {

void CharacterWalker::ApplyMovement(const Vector2& input, const Vector3& gravityUp, float deltaTime, bool isGrounded) {
   auto* transform = GetOwner().GetComponent<TransformComponent>();
   if (!transform) { return; }

   // ScreenSpaceBasis から移動基底を取得
   auto* basis = GetOwner().GetComponent<ScreenSpaceBasis>();
   if (!basis) { return; }
   basis->UpdateBasis(gravityUp);
   Vector3 fProj = basis->GetForwardBasis(gravityUp);
   Vector3 rProj = basis->GetRightBasis(gravityUp);

   // 空中/地上でパラメータを切り替える
   const float accel   = isGrounded ? acceleration    : airAcceleration;
   const float decel   = isGrounded ? friction        : airFriction;

   // 入力から目標方向を計算
   Vector3 inputDir = fProj * input.y + rProj * input.x;
   float   inputLen = inputDir.Length();

   if (inputLen > 1e-6f) {
      // 入力あり: 目標速度へ向けて加速
      Vector3 targetVelocity = inputDir.Normalize() * moveSpeed;
      Vector3 diff = targetVelocity - horizontalVelocity_;
      float   diffLen = diff.Length();
      float   accelStep = accel * deltaTime;
      if (accelStep >= diffLen) {
         horizontalVelocity_ = targetVelocity;
      } else {
         horizontalVelocity_ = horizontalVelocity_ + diff.Normalize() * accelStep;
      }
      lastMoveDirection_ = inputDir.Normalize();
   } else {
      // 入力なし: 摩擦/空気抵抗で減速
      float speed = horizontalVelocity_.Length();
      if (speed > 1e-4f) {
         float decelStep = decel * deltaTime;
         float newSpeed = std::max(0.0f, speed - decelStep);
         horizontalVelocity_ = horizontalVelocity_ * (newSpeed / speed);
      } else {
         horizontalVelocity_ = { 0.0f, 0.0f, 0.0f };
      }
      if (horizontalVelocity_.LengthSquared() < 1e-8f) {
         lastMoveDirection_ = { 0.0f, 0.0f, 0.0f };
      }
   }

   // 位置を更新
   transform->transform.translation =
      transform->transform.translation + horizontalVelocity_ * deltaTime;

   // yaw 補間（移動方向に向きを合わせる）
   if (lastMoveDirection_.LengthSquared() < 1e-6f) { return; }

   auto projectOnPlane = [&](const Vector3& v) -> Vector3 {
      Vector3 proj = v - gravityUp * v.Dot(gravityUp);
      float len = proj.Length();
      return len > 1e-4f ? proj * (1.0f / len) : Vector3{ 0.0f, 0.0f, 0.0f };
   };

   Quaternion currentRotation = transform->transform.GetActiveQuaternion();
   Vector3    currentForward  = RotateVector({ 0.0f, 0.0f, 1.0f }, currentRotation);
   Vector3    curFlat         = projectOnPlane(currentForward);
   Vector3    tgtFlat         = projectOnPlane(lastMoveDirection_);

   if (curFlat.LengthSquared() > 1e-6f && tgtFlat.LengthSquared() > 1e-6f) {
      float cosA  = std::clamp(curFlat.Dot(tgtFlat), -1.0f, 1.0f);
      float angle = std::acos(cosA);
      if (angle > 1e-4f) {
         float      sign     = gravityUp.Dot(curFlat.Cross(tgtFlat)) >= 0.0f ? 1.0f : -1.0f;
         float      step     = std::clamp(turnSpeed * deltaTime, 0.0f, angle);
         Quaternion yawDelta = MakeRotateAxisAngleQuaternion(gravityUp, sign * step);
         transform->transform.SetRotationQuaternion((yawDelta * currentRotation).Normalize());
      }
   }
}

#ifdef USE_IMGUI
void CharacterWalker::DrawInspector() {
   if (!ImGui::CollapsingHeader("CharacterWalker", ImGuiTreeNodeFlags_DefaultOpen)) {
      return;
   }
   ImGui::Separator();
   ImGui::DragFloat("Move Speed",       &moveSpeed,       0.1f, 0.0f, 50.0f);
   ImGui::DragFloat("Acceleration",     &acceleration,    0.5f, 0.0f, 100.0f);
   ImGui::DragFloat("Friction",         &friction,        0.5f, 0.0f, 100.0f);
   ImGui::DragFloat("Air Acceleration", &airAcceleration, 0.5f, 0.0f, 100.0f);
   ImGui::DragFloat("Air Friction",     &airFriction,     0.5f, 0.0f, 100.0f);
   ImGui::DragFloat("Turn Speed",       &turnSpeed,       0.5f, 0.0f, 30.0f);
   ImGui::Spacing();
   ImGui::Text("H.Velocity: (%.2f, %.2f, %.2f)",
      horizontalVelocity_.x, horizontalVelocity_.y, horizontalVelocity_.z);
}
#endif

nlohmann::json CharacterWalker::Serialize() const {
   nlohmann::json json;
   json["moveSpeed"]       = moveSpeed;
   json["acceleration"]    = acceleration;
   json["friction"]        = friction;
   json["airAcceleration"] = airAcceleration;
   json["airFriction"]     = airFriction;
   json["turnSpeed"]       = turnSpeed;
   return json;
}

void CharacterWalker::Deserialize(const nlohmann::json& data) {
   if (data.contains("moveSpeed"))       { moveSpeed       = data["moveSpeed"]; }
   if (data.contains("acceleration"))    { acceleration    = data["acceleration"]; }
   if (data.contains("friction"))        { friction        = data["friction"]; }
   if (data.contains("airAcceleration")) { airAcceleration = data["airAcceleration"]; }
   if (data.contains("airFriction"))     { airFriction     = data["airFriction"]; }
   if (data.contains("turnSpeed"))       { turnSpeed       = data["turnSpeed"]; }
}

} // namespace App
