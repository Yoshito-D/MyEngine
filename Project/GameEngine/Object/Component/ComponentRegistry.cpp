#include "pch.h"
#include "ComponentRegistry.h"

namespace GameEngine {
bool ComponentRegistry::RegisterFactory(const std::string& typeName, Factory factory) {
   if (typeName.empty() || !factory) {
	  return false;
   }

   factories_[typeName] = std::move(factory);
   return true;
}

IObjectComponent* ComponentRegistry::CreateComponent(Object& owner, const std::string& typeName) const {
   auto it = factories_.find(typeName);
   if (it == factories_.end()) {
      return nullptr;
   }

   return it->second(owner);
}

bool ComponentRegistry::HasFactory(const std::string& typeName) const {
   return factories_.contains(typeName);
}

std::vector<std::string> ComponentRegistry::GetRegisteredTypeNames() const {
   std::vector<std::string> names;
   names.reserve(factories_.size());
   for (const auto& [typeName, factory] : factories_) {
      (void)factory;
      names.push_back(typeName);
   }

   std::sort(names.begin(), names.end());
   return names;
}
}