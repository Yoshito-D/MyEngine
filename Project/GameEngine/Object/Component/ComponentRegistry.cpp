#include "pch.h"
#include "ComponentRegistry.h"

#ifdef USE_IMGUI
#include "Utility/ImGuiHelper.h"
#endif

namespace GameEngine {
bool ComponentRegistry::RegisterFactory(
   const std::string& typeName,
   Factory factory,
   ComponentDisplayName displayName
) {
   if (typeName.empty() || !factory) {
	  return false;
   }

   factories_[typeName] = std::move(factory);
#ifdef USE_IMGUI
   if ((displayName.japanese && displayName.japanese[0] != '\0') ||
       (displayName.english && displayName.english[0] != '\0')) {
      displayNames_[typeName] = displayName;
   }
#endif
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

#ifdef USE_IMGUI
std::string ComponentRegistry::GetDisplayName(const std::string& typeName) const {
   const auto it = displayNames_.find(typeName);
   if (it == displayNames_.end()) {
      return typeName;
   }

   return ImGuiHelper::Localize({ it->second.japanese, it->second.english });
}
#endif
}
