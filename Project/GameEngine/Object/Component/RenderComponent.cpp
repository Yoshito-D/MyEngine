#include "pch.h"
#include "RenderComponent.h"

#ifdef USE_IMGUI
#include "Framework/EngineContext.h"
#include "Object.h"
#include "externals/imgui/imgui.h"
#include <algorithm>
#include <cstring>
#endif

namespace GameEngine {

const char* RenderComponent::GetTypeName() const {
   return "RenderComponent";
}

nlohmann::json RenderComponent::Serialize() const {
   return nlohmann::json{
      { "visible", visible },
      { "autoRender", autoRender },
      { "applyPostProcess", applyPostProcess },
      { "textureName", textureName }
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
   if (data.contains("textureName") && data.at("textureName").is_string()) {
      textureName = data.at("textureName").get<std::string>();
   }
}

#ifdef USE_IMGUI
void RenderComponent::DrawInspector(Object& owner) {
   (void)owner;

   if (!ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen)) {
      return;
   }

   ImGui::Checkbox("Visible", &visible);
   ImGui::Checkbox("Auto Render", &autoRender);
   ImGui::Checkbox("Apply PostProcess", &applyPostProcess);

   char textureNameBuffer[256]{};
   const size_t copySize = std::min(textureName.size(), sizeof(textureNameBuffer) - 1);
   std::memcpy(textureNameBuffer, textureName.c_str(), copySize);
   if (ImGui::InputText("Texture", textureNameBuffer, sizeof(textureNameBuffer))) {
      textureName = textureNameBuffer;
   }

   if (auto* texture = EngineContext::GetTexture(textureName)) {
      ImGui::Text("Texture Preview");
      ImTextureID texId = (ImTextureID)(texture->GetTextureSrvHandleGPU().ptr);
      ImGui::Image(texId, ImVec2(96.0f, 96.0f));
   }

   ImGui::Spacing();
}
#endif

}
