#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Object/ObjectType.h"
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

   static ComponentRegistry& GetInstance();

   bool RegisterFactory(const std::string& typeName, Factory factory, ComponentDisplayName displayName = {},
      ObjectTypeMask supportedObjectTypes = kAllObjectTypes);

   IObjectComponent* CreateComponent(Object& owner, const std::string& typeName) const;

   /// @brief 実体を生成せず、登録型と適用可能なObject種別を検証する。
   bool Supports(const std::string& typeName, ObjectType type) const;

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
      Factory factory; ///< 所有オブジェクトに具象コンポーネントを追加する生成処理。
      /// @brief このコンポーネントを接続できるObject種別のビット集合。
      ObjectTypeMask supportedObjectTypes = kAllObjectTypes;
   };

   /// @brief コンポーネント型名ごとの生成処理と接続可能種別。
   std::unordered_map<std::string, Registration> registrations_;
#ifdef USE_IMGUI
   std::unordered_map<std::string, ComponentDisplayName> displayNames_; ///< 登録型名ごとの日本語・英語の表示名。
#endif
};
}
