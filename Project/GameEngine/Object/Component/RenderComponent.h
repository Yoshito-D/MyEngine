#pragma once

#include "IObjectComponent.h"
#include <string>

namespace GameEngine {
class RenderComponent final : public IObjectComponent {
public:
   const char* GetTypeName() const override {
      return "RenderComponent";
   }

   nlohmann::json Serialize() const override {
      return nlohmann::json{
         { "visible", visible },
         { "autoRender", autoRender },
         { "applyPostProcess", applyPostProcess },
         { "textureName", textureName }
      };
   }

   void Deserialize(const nlohmann::json& data) override {
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

   bool visible = true;
   bool autoRender = true;
   bool applyPostProcess = true;
   std::string textureName = "white1x1";
};
}
