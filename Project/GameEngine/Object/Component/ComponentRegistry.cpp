#include "pch.h"
#include "ComponentRegistry.h"
#include "Object.h"

#ifdef USE_IMGUI
#include "Utility/ImGuiHelper.h"
#endif

namespace GameEngine {
bool ComponentRegistry::RegisterFactory(
   const std::string& typeName,
   Factory factory,
   ComponentDisplayName displayName,
   ObjectTypeMask supportedObjectTypes
) {
   if (typeName.empty() || !factory || supportedObjectTypes == 0) {
	  return false;
   }

   registrations_[typeName] = Registration{ std::move(factory), supportedObjectTypes };
#ifdef USE_IMGUI
   if ((displayName.japanese && displayName.japanese[0] != '\0') ||
       (displayName.english && displayName.english[0] != '\0')) {
      displayNames_[typeName] = displayName;
   }
#endif
   return true;
}

IObjectComponent* ComponentRegistry::CreateComponent(Object& owner, const std::string& typeName) const {
   auto it = registrations_.find(typeName);
   if (it == registrations_.end()) {
      return nullptr;
   }

   if ((it->second.supportedObjectTypes & ToObjectTypeMask(owner.GetObjectType())) == 0) {
      // コンポーネントが前提とする描画・UI機能を持たないObject型への追加を拒否する。
      return nullptr;
   }

   return it->second.factory(owner);
}

bool ComponentRegistry::HasFactory(const std::string& typeName) const {
   return registrations_.contains(typeName);
}

std::vector<std::string> ComponentRegistry::GetRegisteredTypeNames() const {
   std::vector<std::string> names;
   names.reserve(registrations_.size());
   for (const auto& [typeName, registration] : registrations_) {
	  (void)registration;
      names.push_back(typeName);
   }

   std::sort(names.begin(), names.end());
   return names;
}

std::vector<std::string> ComponentRegistry::GetRegisteredTypeNames(const Object& owner) const {
   std::vector<std::string> names;
   names.reserve(registrations_.size());
   const ObjectTypeMask ownerType = ToObjectTypeMask(owner.GetObjectType());
   for (const auto& [typeName, registration] : registrations_) {
      if ((registration.supportedObjectTypes & ownerType) != 0) {
         names.push_back(typeName);
      }
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
