#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Math/Functions/MathConstants.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

using namespace GameEngine;

namespace App {

void GravityBody::Update(float deltaTime) {
   // オーナー不在時は更新不可
   if (!HasOwner()) return;

   // 重力有効時のみ物理更新
   if (useGravity) {
      UpdatePhysics(deltaTime);
   }
}

void GravityBody::SetTargetUpVector(const Vector3& targetUp) {
   // 正規化して姿勢目標として保持
   targetUpVector_ = targetUp.Normalize();
}

void GravityBody::ApplyGravityFromSurface(const Vector3& surfaceUp) {
   if (surfaceUp.LengthSquared() < 1e-8f) return;

   targetUpVector_ = surfaceUp.Normalize();
   gravityAcceleration_ = -targetUpVector_ * gravityStrength;
}

void GravityBody::ReleaseGravity() {
   gravityAcceleration_ = {0.0f, 0.0f, 0.0f};
}

Vector3 GravityBody::PredictVelocity(const Vector3& velocity, const Vector3& surfaceUp, float deltaTime) const {
   // 予測は候補惑星の法線を使うため、保持中の重力源と実行時速度には触れない。
   if (!useGravity) {
      return velocity;
   }

   return velocity - surfaceUp * (std::max(0.0f, gravityStrength) * deltaTime);
}

void GravityBody::AddVelocityImpulse(const Vector3& impulse) {
   velocity_ += impulse;
}

void GravityBody::ApplySurfaceVelocity(const Vector3& surfaceVelocity, const Vector3& surfaceUp) {
   // 水平移動・ドリフトの更新で、ジャンプや落下の法線速度を上書きしない。
   const Vector3 normalVelocity = surfaceUp * velocity_.Dot(surfaceUp);
   velocity_ = surfaceVelocity + normalVelocity;
}

void GravityBody::ApplySurfaceDrag(float drag, float deltaTime) {
   const Vector3 normalVelocity = targetUpVector_ * velocity_.Dot(targetUpVector_);
   const Vector3 surfaceVelocity = velocity_ - normalVelocity;
   velocity_ = surfaceVelocity * std::exp(-drag * deltaTime) + normalVelocity;
}

void GravityBody::CancelNormalVelocity(const Vector3& surfaceUp) {
   velocity_ = velocity_ - surfaceUp * velocity_.Dot(surfaceUp);
}

void GravityBody::StopMotion() {
   velocity_ = {0.0f, 0.0f, 0.0f};
}

void GravityBody::UpdatePhysics(float deltaTime) {
   // 位置更新対象Transformを取得
   auto* transform = GetOwner().GetComponent<TransformComponent>();

   if (!transform) return;

   // 重力で速度積分し、速度で位置積分
   velocity_ += gravityAcceleration_ * deltaTime;
   {
      auto pose = transform->GetLocalPose();
      pose.translation = transform->GetLocalPose().translation + velocity_ * deltaTime;
      transform->ApplyLocalPose(pose);
   }
}

#ifdef USE_IMGUI
void GravityBody::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str())) return;

   ImGui::Separator();
   ImGui::Checkbox(Tr("重力を使用", "Use Gravity"), &useGravity);
   ImGui::DragFloat(Tr("重力強度 (m/s²)", "Gravity Strength (m/s²)"), &gravityStrength, 0.1f, 0.0f, 50.0f);
   ImGui::Spacing();
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("現在の上方向", "Current Up"), targetUpVector_.x, targetUpVector_.y,
      targetUpVector_.z);
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("速度", "Velocity"), velocity_.x, velocity_.y, velocity_.z);
}
#endif

nlohmann::json GravityBody::Serialize() const {
   nlohmann::json json;
   json["gravityStrength"] = gravityStrength;
   json["useGravity"] = useGravity;
   return json;
}

nlohmann::json GravityBody::GetSharedParameters() const {
   return ExtractSharedParameters({});
}

void GravityBody::Deserialize(const nlohmann::json& data) {
   ReadParameters(data, true);
}

void GravityBody::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data, false);
}

void GravityBody::ReadParameters(const nlohmann::json& data, [[maybe_unused]] bool initializeRuntime) {
   if (data.contains("gravityStrength")) {
      gravityStrength = data["gravityStrength"];
   }

   if (data.contains("useGravity")) {
      useGravity = data["useGravity"];
   }
}

} // namespace App
