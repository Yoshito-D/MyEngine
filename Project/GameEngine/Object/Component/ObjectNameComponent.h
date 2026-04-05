#pragma once

#include "IObjectComponent.h"
#include <string>

namespace GameEngine {
class ObjectNameComponent final : public IObjectComponent {
public:
   const char* GetTypeName() const override {
      return "ObjectNameComponent";
   }

   nlohmann::json Serialize() const override {
      return nlohmann::json{ { "name", name } };
   }

   void Deserialize(const nlohmann::json& data) override {
      if (data.contains("name") && data.at("name").is_string()) {
         name = data.at("name").get<std::string>();
      }
   }

   std::string name = "Object";
};
}
