#pragma once

#include "IObjectComponent.h"
#include "Object/ObjectType.h"
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
      ComponentDisplayName displayName = {},
      ObjectTypeMask supportedObjectTypes = kAllObjectTypes
   );

   IObjectComponent* CreateComponent(Object& owner, const std::string& typeName) const;

   bool HasFactory(const std::string& typeName) const;

   std::vector<std::string> GetRegisteredTypeNames() const;

   /// @brief 指定オブジェクトへ追加できる登録済みコンポーネント型を取得する
   /// @param owner 追加先オブジェクト
   /// @return 対応しているコンポーネント型名の一覧
   std::vector<std::string> GetRegisteredTypeNames(const Object& owner) const;

#ifdef USE_IMGUI
   std::string GetDisplayName(const std::string& typeName) const;
#endif

private:
   struct Registration {
      Factory factory;
      ObjectTypeMask supportedObjectTypes = kAllObjectTypes;
   };

   std::unordered_map<std::string, Registration> registrations_;
#ifdef USE_IMGUI
   std::unordered_map<std::string, ComponentDisplayName> displayNames_;
#endif
};
}
