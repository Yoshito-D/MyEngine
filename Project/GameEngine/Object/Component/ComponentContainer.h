#pragma once

#include "IObjectComponent.h"
#include "ComponentRegistry.h"
#include <memory>
#include <vector>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <algorithm>
#include <utility>
#include <string>

namespace GameEngine {
class Object;

/// @brief コンポーネントの所有・管理を担当するコンテナクラス
class ComponentContainer {
public:
   ComponentContainer() = default;
   ~ComponentContainer() = default;

   ComponentContainer(const ComponentContainer&) = delete;
   ComponentContainer& operator=(const ComponentContainer&) = delete;
   ComponentContainer(ComponentContainer&&) = default;
   ComponentContainer& operator=(ComponentContainer&&) = default;

   /// @brief コンポーネントを追加する（テンプレート版）
   template <typename T, typename... Args>
   T* Add(Object& owner, Args&&... args) {
      static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");

      const std::type_index type = std::type_index(typeid(T));
      auto it = typeIndex_.find(type);
      if (it != typeIndex_.end()) {
         return static_cast<T*>(it->second);
      }

      auto component = std::make_unique<T>(std::forward<Args>(args)...);
      T* rawPtr = component.get();
      rawPtr->Attach(owner);
      components_.push_back(std::move(component));
      typeIndex_[type] = rawPtr;
      return rawPtr;
   }

   /// @brief コンポーネントを取得する（テンプレート版）
   template <typename T>
   T* Get() const {
      static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");

      const std::type_index type = std::type_index(typeid(T));
      auto it = typeIndex_.find(type);
      if (it == typeIndex_.end()) {
         return nullptr;
      }
      return static_cast<T*>(it->second);
   }

   /// @brief コンポーネントを持っているか（テンプレート版）
   template <typename T>
   bool Has() const {
      static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");
      return typeIndex_.contains(std::type_index(typeid(T)));
   }

   /// @brief コンポーネントを削除する（テンプレート版）
   template <typename T>
   bool Remove() {
      static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");
      return RemoveByTypeIndex(std::type_index(typeid(T)));
   }

   /// @brief 文字列名でコンポーネントを追加する（レジストリ経由）
   IObjectComponent* AddByTypeName(Object& owner, const std::string& typeName);

   /// @brief 文字列名でコンポーネントを持っているか確認する
   bool HasByTypeName(const std::string& typeName) const;

   /// @brief 全コンポーネントを更新する
   void Update(float deltaTime);

   /// @brief 全コンポーネントをクリアする
   void Clear();

   /// @brief シリアライズ
   nlohmann::json Serialize() const;

   /// @brief デシリアライズ
   bool Deserialize(Object& owner, const nlohmann::json& componentsData);

#ifdef USE_IMGUI
   /// @brief インスペクター描画
   void DrawInspector();
#endif

   /// @brief 全コンポーネントのリストを取得
   const std::vector<std::unique_ptr<IObjectComponent>>& GetAll() const { return components_; }

private:
   bool RemoveByTypeIndex(const std::type_index& type);

   std::vector<std::unique_ptr<IObjectComponent>> components_;
   std::unordered_map<std::type_index, IObjectComponent*> typeIndex_;
};

} // namespace GameEngine
