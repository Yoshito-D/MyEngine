#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Object/Component/Base/ComponentRegistry.h"
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

#ifdef USE_IMGUI
/// @brief コンポーネントインスペクターで要求された操作
struct ComponentInspectorAction {
   std::string removedTypeName; ///< 外すコンポーネント型名
   std::string savedTypeName;   ///< プレイ中の値を保存するコンポーネント型名
};
#endif

/// @brief コンポーネントの所有・管理を担当するコンテナクラス
class ComponentContainer {
public:
   /// @brief 指定Objectに所有者を固定したコンテナを生成する
   explicit ComponentContainer(Object& owner) : owner_(owner) {}
   /// @brief 所有する全コンポーネントを破棄する
   ~ComponentContainer() { Clear(); }

   /// @brief コンポーネント所有権の複製を禁止する
   ComponentContainer(const ComponentContainer&) = delete;
   /// @brief コンポーネント所有権のコピー代入を禁止する
   ComponentContainer& operator=(const ComponentContainer&) = delete;
   /// @brief 所有者が変わる移動を禁止する
   ComponentContainer(ComponentContainer&&) = delete;
   /// @brief 所有者が変わる移動代入を禁止する
   ComponentContainer& operator=(ComponentContainer&&) = delete;

   /// @brief コンポーネントを追加する（テンプレート版）
   template <typename T, typename... Args>
   T* Add(Args&&... args) {
      static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");

      const std::type_index type = std::type_index(typeid(T));
      auto it = typeIndex_.find(type);
      if (it != typeIndex_.end()) {
         return static_cast<T*>(it->second);
      }

      auto component = std::make_unique<T>(std::forward<Args>(args)...);
      T* rawPtr = component.get();
      rawPtr->Attach(owner_);
      components_.push_back(std::move(component));
      typeIndex_[type] = rawPtr;
      return rawPtr;
   }

   /// @brief コンポーネントを取得する（テンプレート版）
   template <typename T>
   T* Get() {
      static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");

      const std::type_index type = std::type_index(typeid(T));
      auto it = typeIndex_.find(type);
      if (it == typeIndex_.end()) {
         return nullptr;
      }
      return static_cast<T*>(it->second);
   }

   /// @brief コンポーネントを読み取り専用で取得する（テンプレート版）
   template <typename T>
   const T* Get() const {
      static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");
      const auto it = typeIndex_.find(std::type_index(typeid(T)));
      return it == typeIndex_.end() ? nullptr : static_cast<const T*>(it->second);
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
   IObjectComponent* AddByTypeName(const std::string& typeName);

   /// @brief 文字列名でコンポーネントを持っているか確認する
   bool HasByTypeName(const std::string& typeName) const;

   /// @brief 文字列名でコンポーネントを取得する
   /// @param typeName コンポーネント型名
   /// @return 見つからない場合はnullptr
   IObjectComponent* GetByTypeName(const std::string& typeName);

   /// @brief 文字列名でコンポーネントを読み取り専用で取得する
   /// @return 見つからない場合はnullptr
   const IObjectComponent* GetByTypeName(const std::string& typeName) const;

   /// @brief 文字列名でコンポーネントを削除する
   /// @param typeName コンポーネント型名
   /// @return 削除できた場合はtrue
   bool RemoveByTypeName(const std::string& typeName);

   /// @brief 全コンポーネントを更新する
   void Update(float deltaTime);

   /// @brief 全コンポーネントを有効化し、ライフサイクルを再開する
   void Activate();
   /// @brief 全コンポーネントを無効化し、ライフサイクルを停止する
   void Deactivate();
   /// @brief 全コンポーネントへシーン参照の解決を通知する
   /// @param sceneWorld 参照の解決先
   /// @param initializeRuntime 有効なコンポーネントの実行時状態も初期化する場合はtrue
   void ResolveReferences(SceneWorld& sceneWorld, bool initializeRuntime);

   /// @brief 全コンポーネントをクリアする
   void Clear();

   /// @brief シリアライズ
   nlohmann::json Serialize() const;

   /// @brief JSONの一覧と一致するよう既存コンポーネントを復元する
   /// @param componentsData コンポーネントの完全なJSON配列
   /// @return 有効な配列を適用できた場合はtrue
   bool Deserialize(const nlohmann::json& componentsData);

#ifdef USE_IMGUI
   /// @brief 各コンポーネントのインスペクターと見出し内の操作ボタンを描画する
   /// @param canSaveComponent プレイ中の値を保存するボタンを表示する場合はtrue
   /// @return 保存または削除を要求されたコンポーネント型名
   ComponentInspectorAction DrawInspector(bool canSaveComponent);
#endif

private:
   void ChangeEnabledState(bool enabled);
   bool RemoveByTypeIndex(const std::type_index& type);

   Object& owner_;
   std::vector<std::unique_ptr<IObjectComponent>> components_;
   std::unordered_map<std::type_index, IObjectComponent*> typeIndex_;
};

} // namespace GameEngine
