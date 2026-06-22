#include "pch.h"
#include "RenderComponent.h"
#include "ComponentRegistry.h"
#include "Object.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
   const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
      GameEngine::RenderComponent::kTypeName,
      [](GameEngine::Object& o) -> GameEngine::IObjectComponent* { return o.AddComponent<GameEngine::RenderComponent>(); }
   );
}

namespace GameEngine {

const char* RenderComponent::GetTypeName() const {
   return "RenderComponent";
}

nlohmann::json RenderComponent::Serialize() const {
   return nlohmann::json{
      { "visible", visible },
      { "autoRender", autoRender },
      { "applyPostProcess", applyPostProcess }
   };
}

void RenderComponent::Deserialize(const nlohmann::json& data) {
   if (data.contains("visible") && data.at("visible").is_boolean()) {
      visible = data.at("visible").get<bool>();
   }
   if (data.contains("autoRender") && data.at("autoRender").is_boolean()) {
      autoRender = data.at("autoRender").get<bool>();
   }
   if (data.contains("applyPostProcess") && data.at("applyPostProcess").is_boolean()) {
      applyPostProcess = data.at("applyPostProcess").get<bool>();
   }
}

#ifdef USE_IMGUI
void RenderComponent::DrawInspector() {
   if (!ImGui::CollapsingHeader("Render")) {
      return;
   }

   ImGui::Checkbox("Visible", &visible);
   ImGui::Checkbox("Auto Render", &autoRender);
   ImGui::Checkbox("Apply PostProcess", &applyPostProcess);

   ImGui::Spacing();
}
#endif

}
