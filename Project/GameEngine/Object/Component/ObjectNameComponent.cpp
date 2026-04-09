#include "pch.h"
#include "ObjectNameComponent.h"

namespace GameEngine {

const char* ObjectNameComponent::GetTypeName() const {
   return "ObjectNameComponent";
}

nlohmann::json ObjectNameComponent::Serialize() const {
   return nlohmann::json{ { "name", name } };
}

void ObjectNameComponent::Deserialize(const nlohmann::json& data) {
   if (data.contains("name") && data.at("name").is_string()) {
      name = data.at("name").get<std::string>();
   }
}

}
