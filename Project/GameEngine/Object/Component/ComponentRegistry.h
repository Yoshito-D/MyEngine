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

   bool RegisterFactory(const std::string& typeName, Factory factory);

   IObjectComponent* CreateComponent(Object& owner, const std::string& typeName) const;

   bool HasFactory(const std::string& typeName) const;

   std::vector<std::string> GetRegisteredTypeNames() const;

private:
   std::unordered_map<std::string, Factory> factories_;
};
}
