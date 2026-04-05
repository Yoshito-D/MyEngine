#pragma once

#include "IObjectComponent.h"
#include <functional>
#include <string>
#include <unordered_map>

namespace GameEngine {
class Object;

class ComponentRegistry {
public:
   using Factory = std::function<IObjectComponent*(Object&)>;

   static ComponentRegistry& GetInstance() {
      static ComponentRegistry instance;
      return instance;
   }

   bool RegisterFactory(const std::string& typeName, Factory factory) {
      if (typeName.empty() || !factory) {
         return false;
      }

      factories_[typeName] = std::move(factory);
      return true;
   }

   IObjectComponent* CreateComponent(Object& owner, const std::string& typeName) const {
      auto it = factories_.find(typeName);
      if (it == factories_.end()) {
         return nullptr;
      }

      return it->second(owner);
   }

   bool HasFactory(const std::string& typeName) const {
      return factories_.contains(typeName);
   }

private:
   std::unordered_map<std::string, Factory> factories_;
};
}
