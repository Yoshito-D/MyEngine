#include "GameEngine/pch.h"
#include "GameEngine/Utility/JsonValue.h"
#include "GameEngine/Scene/Camera/Components/LookAtAim.h"
#include "GameEngine/Scene/Camera/Core/VirtualCamera.h"
#include "GameEngine/Math/MathUtils.h"
#include <cmath>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace GameEngine {

namespace {
const bool kRegistered =
   VirtualCamera::RegisterComponentFactory("LookAtAim", [](VirtualCamera& camera) -> ICinemachineComponent* {
      if (auto* existing = camera.GetComponent<LookAtAim>()) {
         return existing;
      }

      return camera.AddComponent<LookAtAim>();
   });

} // namespace

void LookAtAim::MutateCameraState(CameraState& state, float deltaTime) {
   if (GetOwnerCamera() == nullptr) return;

   const Transform* target = GetOwnerCamera()->GetLookAtTarget();

   if (target == nullptr) return;

   // ターゲット方向を計算
   Vector3 lookAtPosition = target->translation + offset_;
   Vector3 direction = lookAtPosition - state.transform.translation;

   if (direction.LengthSquared() < 1e-6f) return;

   direction = direction.Normalize();

   // 目標の回転を計算
   Quaternion targetRotation = LookRotation(direction, Vector3(0.0f, 1.0f, 0.0f));

   // ダンピング付きで回転を補間
   float t = 1.0f - std::exp(-damping_ * deltaTime);
   Quaternion newRotation = Quaternion::Slerp(state.transform.GetActiveQuaternion(), targetRotation, t);
   state.transform.SetRotationQuaternion(newRotation);
}

nlohmann::json LookAtAim::Serialize() const {
   return nlohmann::json{{"damping", damping_}, {"offset", GameEngine::JsonValue::WriteVector3(offset_)}};
}

nlohmann::json LookAtAim::GetSharedParameters() const {
   return ExtractSharedParameters({});
}

void LookAtAim::Deserialize(const nlohmann::json& data) {
   ReadParameters(data, true);
}

void LookAtAim::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data, false);
}

void LookAtAim::ReadParameters(const nlohmann::json& data, [[maybe_unused]] bool initializeRuntime) {
   if (!data.is_object()) return;

   damping_ = GameEngine::JsonValue::ReadFloat(data, "damping", damping_);

   if (data.contains("offset")) {
      offset_ = GameEngine::JsonValue::ReadVector3(data.at("offset"), offset_);
   }
}

#ifdef USE_IMGUI
void LookAtAim::DrawInspector() {
   bool enabled = IsEnabled();

   if (ImGui::Checkbox("Enabled", &enabled)) {
      SetEnabled(enabled);
   }

   if (ImGui::DragFloat("Damping", &damping_, 0.1f, 0.0f, 50.0f)) {
   }

   float off[3] = {offset_.x, offset_.y, offset_.z};

   if (ImGui::DragFloat3("Offset", off, 0.1f)) {
      offset_ = {off[0], off[1], off[2]};
   }
}
#endif

} // namespace GameEngine
