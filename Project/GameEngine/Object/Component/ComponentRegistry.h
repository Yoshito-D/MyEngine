#pragma once

#include "IObjectComponent.h"
#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

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

   std::vector<std::string> GetRegisteredTypeNames() const {
      std::vector<std::string> names;
      names.reserve(factories_.size());
      for (const auto& [typeName, factory] : factories_) {
         (void)factory;
         names.push_back(typeName);
      }

      std::sort(names.begin(), names.end());
      return names;
   }

private:
   std::unordered_map<std::string, Factory> factories_;
};
}
