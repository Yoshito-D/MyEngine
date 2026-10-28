#include "GameEngine/pch.h"
#include "GameEngine/Utility/JsonValue.h"
#include "GameEngine/Scene/Camera/Components/FollowBody.h"
#include "GameEngine/Scene/Camera/Core/VirtualCamera.h"
#include <cmath>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace GameEngine {

namespace {
const bool kRegistered =
   VirtualCamera::RegisterComponentFactory("FollowBody", [](VirtualCamera& camera) -> ICinemachineComponent* {
      if (auto* existing = camera.GetComponent<FollowBody>()) {
         return existing;
      }

      return camera.AddComponent<FollowBody>();
   });

} // namespace

void FollowBody::MutateCameraState(CameraState& state, float deltaTime) {
   if (GetOwnerCamera() == nullptr) return;

   const Transform* target = GetOwnerCamera()->GetFollowTarget();

   if (target == nullptr) return;

   // ターゲットのワールド位置 + オフセット
   Vector3 targetPosition = target->translation + offset_;

   // ダンピング付きの追従（指数減衰）
   float dampX = 1.0f - std::exp(-damping_.x * deltaTime);
   float dampY = 1.0f - std::exp(-damping_.y * deltaTime);
   float dampZ = 1.0f - std::exp(-damping_.z * deltaTime);

   state.transform.translation.x += (targetPosition.x - state.transform.translation.x) * dampX;
   state.transform.translation.y += (targetPosition.y - state.transform.translation.y) * dampY;
   state.transform.translation.z += (targetPosition.z - state.transform.translation.z) * dampZ;
}

nlohmann::json FollowBody::Serialize() const {
   return nlohmann::json{{"offset", GameEngine::JsonValue::WriteVector3(offset_)},
      {"damping", GameEngine::JsonValue::WriteVector3(damping_)}};
}

nlohmann::json FollowBody::GetSharedParameters() const {
   return ExtractSharedParameters({});
}

void FollowBody::Deserialize(const nlohmann::json& data) {
   ReadParameters(data, true);
}

void FollowBody::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data, false);
}

void FollowBody::ReadParameters(const nlohmann::json& data, [[maybe_unused]] bool initializeRuntime) {
   if (!data.is_object()) return;

   if (data.contains("offset")) {
      offset_ = GameEngine::JsonValue::ReadVector3(data.at("offset"), offset_);
   }

   if (data.contains("damping")) {
      damping_ = GameEngine::JsonValue::ReadVector3(data.at("damping"), damping_);
   }
}

#ifdef USE_IMGUI
void FollowBody::DrawInspector() {
   bool enabled = IsEnabled();

   if (ImGui::Checkbox("Enabled", &enabled)) {
      SetEnabled(enabled);
   }

   float off[3] = {offset_.x, offset_.y, offset_.z};

   if (ImGui::DragFloat3("Offset", off, 0.1f)) {
      offset_ = {off[0], off[1], off[2]};
   }

   float damp[3] = {damping_.x, damping_.y, damping_.z};

   if (ImGui::DragFloat3("Damping", damp, 0.1f, 0.0f, 50.0f)) {
      damping_ = {damp[0], damp[1], damp[2]};
   }
}
#endif

} // namespace GameEngine
