#include "GravityBody.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

using namespace GameEngine;

namespace App {

void GravityBody::Update(float deltaTime) {
   // オーナー不在時は更新不可
   if (!HasOwner()) {
      return;
   }

   // 目標Up方向へ姿勢を追従
   UpdateRotation(deltaTime);

   // 重力有効時のみ物理更新
   if (useGravity) {
      UpdatePhysics(deltaTime);
   }
}

void GravityBody::SetTargetUpVector(const Vector3& targetUp) {
   // 正規化して姿勢目標として保持
   targetUpVector_ = targetUp.Normalize();
}

void GravityBody::SetGravity(const Vector3& gravity) {
   // 外部計算済み重力加速度を保持
   gravityAcceleration_ = gravity;
}

void GravityBody::SnapToUpVector(const Vector3& targetUp) {
   auto* transform = GetOwner().GetComponent<TransformComponent>();
   if (!transform) { return; }

   Vector3 newUp = targetUp.Normalize();
   if (newUp.LengthSquared() < 1e-8f) { return; }

   Vector3 oldUp = currentUpVector_.Normalize();
   currentUpVector_ = newUp;
   targetUpVector_  = newUp;

   // 現在のforwardをold upで平面投影してnew upで再構築
   Quaternion cur = transform->transform.GetActiveQuaternion();
   Vector3 forward = RotateVector({ 0.0f, 0.0f, 1.0f }, cur);
   Vector3 flatFwd = forward - oldUp * oldUp.Dot(forward);
   float flatLen = flatFwd.Length();
   if (flatLen < 1e-4f) {
      Vector3 tmp = (std::abs(newUp.x) < 0.9f) ? Vector3{1,0,0} : Vector3{0,0,1};
      flatFwd = tmp - newUp * newUp.Dot(tmp);
      flatLen = flatFwd.Length();
   }
   if (flatLen < 1e-4f) { return; }
   flatFwd = flatFwd * (1.0f / flatLen);
   // flatFwd を new up 平面に投影
   flatFwd = flatFwd - newUp * newUp.Dot(flatFwd);
   flatLen = flatFwd.Length();
   if (flatLen < 1e-4f) { return; }
   flatFwd = flatFwd * (1.0f / flatLen);

   Vector3 right = newUp.Cross(flatFwd).Normalize();
   Vector3 fwd   = right.Cross(newUp).Normalize();

   // right, newUp, fwd から回転行列→クォータニオン
   // row-basis: x=right, y=newUp, z=fwd
   float m00 = right.x, m10 = right.y, m20 = right.z;
   float m01 = newUp.x, m11 = newUp.y, m21 = newUp.z;
   float m02 = fwd.x,   m12 = fwd.y,   m22 = fwd.z;
   float trace = m00 + m11 + m22;
   Quaternion q;
   if (trace > 0.0f) {
      float s = 0.5f / std::sqrt(trace + 1.0f);
      q.w = 0.25f / s;
      q.x = (m21 - m12) * s;
      q.y = (m02 - m20) * s;
      q.z = (m10 - m01) * s;
   } else if (m00 > m11 && m00 > m22) {
      float s = 2.0f * std::sqrt(1.0f + m00 - m11 - m22);
      q.w = (m21 - m12) / s;
      q.x = 0.25f * s;
      q.y = (m01 + m10) / s;
      q.z = (m02 + m20) / s;
   } else if (m11 > m22) {
      float s = 2.0f * std::sqrt(1.0f + m11 - m00 - m22);
      q.w = (m02 - m20) / s;
      q.x = (m01 + m10) / s;
      q.y = 0.25f * s;
      q.z = (m12 + m21) / s;
   } else {
      float s = 2.0f * std::sqrt(1.0f + m22 - m00 - m11);
      q.w = (m10 - m01) / s;
      q.x = (m02 + m20) / s;
      q.y = (m12 + m21) / s;
      q.z = 0.25f * s;
   }
   transform->transform.SetRotationQuaternion(q.Normalize());
}

void GravityBody::UpdateRotation(float deltaTime) {
   // 回転対象Transformを取得
   auto* transform = GetOwner().GetComponent<TransformComponent>();
   if (!transform) { return; }

   // 現在Upと目標Upを正規化
   Vector3 current = currentUpVector_.Normalize();
   Vector3 target  = targetUpVector_.Normalize();

   if (current.LengthSquared() < 1e-8f) { current = Vector3{ 0.0f, 1.0f, 0.0f }; currentUpVector_ = current; }
   if (target.LengthSquared()  < 1e-8f) { target  = Vector3{ 0.0f, 1.0f, 0.0f }; targetUpVector_  = target; }

   // 方向差が小さい場合は補間不要
   float dot = current.Dot(target);
   if (dot > 0.9999f) { return; }

   // ほぼ逆向きの場合は180度回転の特別処理
   if (dot < -0.9999f) {
      Vector3 axis = Vector3{ 1.0f, 0.0f, 0.0f }.Cross(current);
      if (axis.LengthSquared() < 1e-6f) { axis = Vector3{ 0.0f, 1.0f, 0.0f }.Cross(current); }
      if (axis.LengthSquared() < 1e-6f) { axis = Vector3{ 0.0f, 0.0f, 1.0f }.Cross(current); }
      if (axis.LengthSquared() < 1e-6f) { return; }
      axis = axis.Normalize();
      Quaternion rotDelta = MakeRotateAxisAngleQuaternion(axis, 3.14159265358979323846f);
      Quaternion cur      = transform->transform.GetActiveQuaternion();
      transform->transform.SetRotationQuaternion((rotDelta * cur).Normalize());
      currentUpVector_ = target;
      return;
   }

   // 回転軸を求める
   Vector3 rotAxis = current.Cross(target);
   if (rotAxis.LengthSquared() < 1e-6f) { return; }
   rotAxis = rotAxis.Normalize();

   // 角度差を算出
   float angle = std::acos(std::clamp(dot, -1.0f, 1.0f));
   if (std::abs(angle) < 1e-6f) { return; }

   // rotationSpeed に基づき段階的に回転
   float t = std::clamp(rotationSpeed * deltaTime, 0.0f, 1.0f);
   Quaternion rotDelta = MakeRotateAxisAngleQuaternion(rotAxis, angle * t);
   Quaternion cur      = transform->transform.GetActiveQuaternion();
   transform->transform.SetRotationQuaternion((rotDelta * cur).Normalize());

   // 現在Upも補間更新
   if (t >= 0.9999f) {
      currentUpVector_ = target;
   } else {
      currentUpVector_ = Vector3::Lerp(current, target, t);
   }
}

void GravityBody::UpdatePhysics(float deltaTime) {
   // 位置更新対象Transformを取得
   auto* transform = GetOwner().GetComponent<TransformComponent>();
   if (!transform) { return; }

   // 重力で速度積分し、速度で位置積分
   velocity_ += gravityAcceleration_ * deltaTime;
   transform->transform.translation = transform->transform.translation + velocity_ * deltaTime;
}

#ifdef USE_IMGUI
void GravityBody::DrawInspector() {
   if (!ImGui::CollapsingHeader("GravityBody")) {
      return;
   }
   ImGui::Separator();
   ImGui::Checkbox("Use Gravity", &useGravity);
   ImGui::DragFloat("Rotation Speed",   &rotationSpeed,   0.1f, 0.1f, 20.0f);
   ImGui::DragFloat("Gravity Strength", &gravityStrength, 0.1f, 0.0f, 50.0f);
   ImGui::Spacing();
   ImGui::Text("Current Up: (%.2f, %.2f, %.2f)", currentUpVector_.x, currentUpVector_.y, currentUpVector_.z);
   ImGui::Text("Velocity:   (%.2f, %.2f, %.2f)", velocity_.x, velocity_.y, velocity_.z);
}
#endif

nlohmann::json GravityBody::Serialize() const {
   nlohmann::json json;
   json["rotationSpeed"]   = rotationSpeed;
   json["gravityStrength"] = gravityStrength;
   json["useGravity"]      = useGravity;
   json["currentUpVector"] = { currentUpVector_.x, currentUpVector_.y, currentUpVector_.z };
   json["velocity"]        = { velocity_.x, velocity_.y, velocity_.z };
   return json;
}

void GravityBody::Deserialize(const nlohmann::json& data) {
   if (data.contains("rotationSpeed"))   { rotationSpeed   = data["rotationSpeed"]; }
   if (data.contains("gravityStrength")) { gravityStrength  = data["gravityStrength"]; }
   if (data.contains("useGravity"))      { useGravity       = data["useGravity"]; }
   if (data.contains("currentUpVector")) {
      auto up = data["currentUpVector"];
      currentUpVector_ = { up[0], up[1], up[2] };
      targetUpVector_  = currentUpVector_;
   }
   if (data.contains("velocity")) {
      auto vel = data["velocity"];
      velocity_ = { vel[0], vel[1], vel[2] };
   }
}

} // namespace App
