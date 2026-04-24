#include "GravityAttractorLink.h"
#include "GravityBody.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace App {

void GravityAttractorLink::Update(float /*deltaTime*/) {
   if (!HasOwner() || !attractor_) { return; }

   auto* gravityBody = GetOwner().GetComponent<GravityBody>();
   auto* transform   = GetOwner().GetComponent<GameEngine::TransformComponent>();
   if (!gravityBody || !transform) { return; }

   attractor_->ApplyTo(*gravityBody, transform->transform.translation);
}

#ifdef USE_IMGUI
void GravityAttractorLink::DrawInspector() {
   if (!ImGui::CollapsingHeader("GravityAttractorLink")) {
      return;
   }
   ImGui::Separator();
   ImGui::Text("Attractor: %s", attractor_ ? "Set" : "None");
}
#endif

} // namespace App
