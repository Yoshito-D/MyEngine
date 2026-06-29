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

   bool RegisterFactory(
      const std::string& typeName,
      Factory factory,
      ComponentDisplayName displayName = {}
   );

   IObjectComponent* CreateComponent(Object& owner, const std::string& typeName) const;

   bool HasFactory(const std::string& typeName) const;

   std::vector<std::string> GetRegisteredTypeNames() const;

#ifdef USE_IMGUI
   std::string GetDisplayName(const std::string& typeName) const;
#endif

private:
   std::unordered_map<std::string, Factory> factories_;
#ifdef USE_IMGUI
   std::unordered_map<std::string, ComponentDisplayName> displayNames_;
#endif
};
}
