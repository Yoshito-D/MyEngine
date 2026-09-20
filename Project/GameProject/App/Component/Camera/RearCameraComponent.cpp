#include "RearCameraComponent.h"
#include "PlayerRearFollowCamera.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace App {

void RearCameraComponent::Initialize(GameEngine::VirtualCamera* owner) {
   ICinemachineComponent::Initialize(owner);
   if (auto* camera = owner ? owner->GetComponent<PlayerRearFollowCamera>() : nullptr) {
      Reset(*camera);
   }
}

void RearCameraComponent::Deserialize(const nlohmann::json&) {
   if (auto* camera = owner_ ? owner_->GetComponent<PlayerRearFollowCamera>() : nullptr) {
      Reset(*camera);
   }
}

PlayerRearFollowCamera* RearCameraComponent::GetRearCamera() const {
   auto* camera = owner_ ? owner_->GetComponent<PlayerRearFollowCamera>() : nullptr;
   // 兄弟部品への永続ポインターを持たず、Inspectorから削除された場合も安全に停止する。
   return camera && camera->IsEnabled() && camera->HasRequiredComponents() ? camera : nullptr;
}

#ifdef USE_IMGUI
void RearCameraComponent::DrawInspector() {
   ImGui::Checkbox("Enabled", &isEnabled_);
   ImGui::Text("Execution order: %d", GetExecutionOrder());
}
#endif

} // namespace App
