#include "GameProject/App/Component/Camera/RearFollow/RearCameraComponent.h"
#include "GameProject/App/Component/Camera/RearFollow/PlayerRearFollowCamera.h"
#include "GameEngine/Scene/Camera/Core/VirtualCamera.h"
#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace App {

void RearCameraComponent::OnAttach() {
   auto* owner = GetOwnerCamera();
   if (auto* camera = owner ? owner->GetComponent<PlayerRearFollowCamera>() : nullptr) {
      Reset(camera->DescribeSettings());
   }
}

void RearCameraComponent::Deserialize(const nlohmann::json&) {
   if (auto* camera = GetOwnerCamera() ? GetOwnerCamera()->GetComponent<PlayerRearFollowCamera>() : nullptr) {
      Reset(camera->DescribeSettings());
   }
}

PlayerRearFollowCamera* RearCameraComponent::GetRearCamera() {
   auto* camera = GetOwnerCamera() ? GetOwnerCamera()->GetComponent<PlayerRearFollowCamera>() : nullptr;
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
