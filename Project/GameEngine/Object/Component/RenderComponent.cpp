#include "pch.h"
#include "RenderComponent.h"

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

}
