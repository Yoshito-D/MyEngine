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
   // Transform が無い場合は移動不可
   auto* transform = GetOwner().GetComponent<TransformComponent>();
   if (!transform) { return; }

   // ScreenSpaceBasis から重力平面上の移動基底を取得
   auto* basis = GetOwner().GetComponent<ScreenSpaceBasis>();
   if (!basis) { return; }
   basis->UpdateBasis(gravityUp);
   Vector3 fProj = basis->GetForwardBasis(gravityUp);
   Vector3 rProj = basis->GetRightBasis(gravityUp);

   // 接地状態で加減速特性を切り替え
   const float accel   = isGrounded ? acceleration    : airAcceleration;
   const float decel   = isGrounded ? friction        : airFriction;

   // 入力ベクトルを移動方向へ変換
   Vector3 inputDir = fProj * input.y + rProj * input.x;
   float   inputLen = inputDir.Length();

   if (inputLen > 1e-6f) {
      // 入力時: 目標速度へ加速
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
      // 無入力時: 減速して慣性を収束
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

   // 水平速度で位置更新
   transform->transform.translation =
      transform->transform.translation + horizontalVelocity_ * deltaTime;

   // 移動方向が無ければ向き補間は不要
   if (lastMoveDirection_.LengthSquared() < 1e-6f) { return; }

   // 重力平面投影ヘルパー
   auto projectOnPlane = [&](const Vector3& v) -> Vector3 {
      Vector3 proj = v - gravityUp * v.Dot(gravityUp);
      float len = proj.Length();
      return len > 1e-4f ? proj * (1.0f / len) : Vector3{ 0.0f, 0.0f, 0.0f };
   };

   // 現在前方と目標前方の平面内角度を計算
   Quaternion currentRotation = transform->transform.GetActiveQuaternion();
   Vector3    currentForward  = RotateVector({ 0.0f, 0.0f, 1.0f }, currentRotation);
   Vector3    curFlat         = projectOnPlane(currentForward);
   Vector3    tgtFlat         = projectOnPlane(lastMoveDirection_);

   // yaw を段階的に補間して進行方向へ向ける
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
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }
   ImGui::Separator();
   ImGui::DragFloat(Tr("移動速度", "Move Speed"),       &moveSpeed,       0.1f, 0.0f, 50.0f);
   ImGui::DragFloat(Tr("加速度", "Acceleration"),     &acceleration,    0.5f, 0.0f, 100.0f);
   ImGui::DragFloat(Tr("摩擦", "Friction"),         &friction,        0.5f, 0.0f, 100.0f);
   ImGui::DragFloat(Tr("空中加速度", "Air Acceleration"), &airAcceleration, 0.5f, 0.0f, 100.0f);
   ImGui::DragFloat(Tr("空中摩擦", "Air Friction"),     &airFriction,     0.5f, 0.0f, 100.0f);
   ImGui::DragFloat(Tr("旋回速度", "Turn Speed"),       &turnSpeed,       0.5f, 0.0f, 30.0f);
   ImGui::Spacing();
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("水平速度", "Horizontal Velocity"),
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
