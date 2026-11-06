#include "GameProject/App/Component/Title/TitleCameraRotation.h"

#include "GameEngine/Math/Functions/MathConstants.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include "GameEngine/Scene/Camera/Core/VirtualCamera.h"
#include "GameEngine/Utility/JsonValue.h"
#include <cmath>

#ifdef USE_IMGUI
#include "imgui.h"
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#endif

namespace App {
namespace {
const bool kRegistered = GameEngine::VirtualCamera::RegisterComponentFactory(
   "TitleCameraRotation", [](GameEngine::VirtualCamera& camera) -> GameEngine::ICinemachineComponent* {
      if (auto* existing = camera.GetComponent<TitleCameraRotation>()) {
         return existing;
      }

      return camera.AddComponent<TitleCameraRotation>();
   });
} // namespace

void TitleCameraRotation::MutateCameraState(GameEngine::CameraState& state, float deltaTime) {
   if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return;

   const float angle = rotationSpeed_ * deltaTime;

   if (!std::isfinite(angle)) return;

   // ワールドY軸の回転を左から合成し、Euler角の特異点と長時間再生時の角度蓄積を避ける。
   const auto rotation = GameEngine::MakeRotateAxisAngleQuaternion({0.0f, 1.0f, 0.0f}, angle);
   state.transform.SetRotationQuaternion(rotation * state.transform.GetActiveQuaternion());
   state.hasViewMatrixOverride = false;
}

nlohmann::json TitleCameraRotation::Serialize() const {
   return nlohmann::json{{"rotationSpeed", rotationSpeed_}};
}

void TitleCameraRotation::Deserialize(const nlohmann::json& data) {
   rotationSpeed_ = GameEngine::JsonValue::ReadFloat(data, "rotationSpeed", rotationSpeed_);
}

#ifdef USE_IMGUI
void TitleCameraRotation::DrawInspector() {
   float degreesPerSecond = GameEngine::ImGuiHelper::RadiansToDegrees(rotationSpeed_);

   if (ImGui::DragFloat("Rotation Speed (deg/s)", &degreesPerSecond, 0.1f, -30.0f, 30.0f)) {
      rotationSpeed_ = GameEngine::ImGuiHelper::DegreesToRadians(degreesPerSecond);
   }
}
#endif

} // namespace App
