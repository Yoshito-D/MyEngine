#include "CameraGravityBridge.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void CameraGravityBridge::Update(float /*deltaTime*/) {
   if (!HasOwner()) { return; }

   auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   if (!transform) { return; }

   GameEngine::Vector3 toSelf = transform->transform.translation - planetCenter_;
   float len = toSelf.Length();
   if (len < 1e-4f) { return; }
   GameEngine::Vector3 gravityUp = toSelf * (1.0f / len);
   GameEngine::Vector3 pos       = transform->transform.translation;

   if (gravityFollowCamera_) {
      gravityFollowCamera_->SetGravityUp(gravityUp);
      gravityFollowCamera_->SetPivotTarget(pos);
   }
   if (planetLeashCamera_) {
      planetLeashCamera_->SetGravityUp(gravityUp);
      planetLeashCamera_->SetPivotTarget(pos);
      planetLeashCamera_->SetSphereCenter(planetCenter_);
   }
}

#ifdef USE_IMGUI
void CameraGravityBridge::DrawInspector() {
   if (!ImGui::CollapsingHeader("CameraGravityBridge")) {
      return;
   }
   ImGui::Separator();
   ImGui::Text("Planet Center: (%.2f, %.2f, %.2f)",
      planetCenter_.x, planetCenter_.y, planetCenter_.z);
   ImGui::Text("GravityFollowCamera: %s", gravityFollowCamera_ ? "Set" : "None");
   ImGui::Text("PlanetLeashCamera:   %s", planetLeashCamera_   ? "Set" : "None");
}
#endif

} // namespace App
