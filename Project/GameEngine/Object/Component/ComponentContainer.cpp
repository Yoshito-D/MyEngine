#include "pch.h"
#include "ComponentContainer.h"
#include "Object.h"
#include <algorithm>

namespace GameEngine {

IObjectComponent* ComponentContainer::AddByTypeName(Object& owner, const std::string& typeName) {
   if (typeName.empty()) {
      return nullptr;
   }

   return ComponentRegistry::GetInstance().CreateComponent(owner, typeName);
}

bool ComponentContainer::HasByTypeName(const std::string& typeName) const {
   if (typeName.empty()) {
      return false;
   }

   for (const auto& component : components_) {
      if (!component) {
         continue;
      }
      if (typeName == component->GetTypeName()) {
         return true;
      }
   }

   return false;
}

void ComponentContainer::Update(float deltaTime) {
   for (auto& component : components_) {
      if (!component || !component->IsEnabled()) {
         continue;
      }
      component->Update(deltaTime);
   }
}

void ComponentContainer::Clear() {
   for (auto& component : components_) {
      if (component) {
         component->Detach();
      }
   }
   components_.clear();
   typeIndex_.clear();
}

nlohmann::json ComponentContainer::Serialize() const {
   nlohmann::json componentsData = nlohmann::json::array();

   for (const auto& component : components_) {
      if (!component) {
         continue;
      }

      nlohmann::json entry = nlohmann::json::object();
      entry["typeName"] = component->GetTypeName();
      entry["enabled"] = component->IsEnabled();
      entry["data"] = component->Serialize();

      componentsData.push_back(std::move(entry));
   }

   return componentsData;
}

bool ComponentContainer::Deserialize(Object& owner, const nlohmann::json& componentsData) {
   if (!componentsData.is_array()) {
      return false;
   }

   bool hasAppliedAnyComponent = false;
   for (const auto& componentData : componentsData) {
      if (!componentData.is_object()) {
         continue;
      }

      const std::string typeName = componentData.value("typeName", "");
      if (typeName.empty()) {
         continue;
      }

      auto* component = AddByTypeName(owner, typeName);
      if (!component) {
         continue;
      }

      if (componentData.contains("enabled") && componentData.at("enabled").is_boolean()) {
         component->SetEnabled(componentData.at("enabled").get<bool>());
      }

      if (componentData.contains("data") && componentData.at("data").is_object()) {
         component->Deserialize(componentData.at("data"));
      }

      hasAppliedAnyComponent = true;
   }

   return hasAppliedAnyComponent;
}

#ifdef USE_IMGUI
void ComponentContainer::DrawInspector() {
   for (auto& component : components_) {
      if (!component) {
         continue;
      }
      component->DrawInspector();
   }
}
#endif

bool ComponentContainer::RemoveByTypeIndex(const std::type_index& type) {
   auto mapIt = typeIndex_.find(type);
   if (mapIt == typeIndex_.end()) {
      return false;
   }

   IObjectComponent* target = mapIt->second;
   typeIndex_.erase(mapIt);

   auto vecIt = std::find_if(components_.begin(), components_.end(),
      [target](const std::unique_ptr<IObjectComponent>& component) {
         return component.get() == target;
      });

   if (vecIt != components_.end()) {
      (*vecIt)->Detach();
      components_.erase(vecIt);
      return true;
   }

   return false;
}

} // namespace GameEngine
