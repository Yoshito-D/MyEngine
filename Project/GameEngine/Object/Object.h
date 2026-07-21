#pragma once
#include "Component/ComponentContainer.h"
#include "Component/IObjectComponent.h"
#include "ObjectType.h"
#include <memory>
#include <string>

namespace GameEngine {
class Camera;
class TransformComponent;

/// @brief ゲームオブジェクトの基底クラス
/// コンポーネントの管理はComponentContainerに委譲する
class Object {
public:
   Object();
   virtual ~Object() = default;

   /// @brief 実体のオブジェクト種別を取得する
   /// @return コンポーネント互換性の判定に使用する種別
   virtual ObjectType GetObjectType() const { return ObjectType::Generic; }

   // --- コンポーネント操作（ComponentContainerへ委譲） ---

   template <typename T, typename... Args>
   T* AddComponent(Args&&... args) {
	  return components_.Add<T>(*this, std::forward<Args>(args)...);
   }

   template <typename T>
   T* GetComponent() {
	  return components_.Get<T>();
   }

   template <typename T>
   const T* GetComponent() const {
	  return components_.Get<T>();
   }

   template <typename T>
   bool HasComponent() const {
	  return components_.Has<T>();
   }

   template <typename T>
   bool RemoveComponent() {
	  return components_.Remove<T>();
   }

   IObjectComponent* AddComponentByTypeName(const std::string& typeName);
   bool HasComponentByTypeName(const std::string& typeName) const;

   /// @brief 型名からコンポーネントを取得する
   /// @param typeName コンポーネント型名
   /// @return 見つからない場合はnullptr
   IObjectComponent* GetComponentByTypeName(const std::string& typeName) const;

   /// @brief 型名を指定してコンポーネントを外す
   /// @param typeName コンポーネント型名
   /// @return 削除できた場合はtrue
   bool RemoveComponentByTypeName(const std::string& typeName);

   // --- シリアライゼーション ---

   bool DeserializeComponents(const nlohmann::json& componentsData);
   nlohmann::json SerializeComponents() const;

   // --- ライフサイクル ---

   void UpdateComponents(float deltaTime);

   void SetObjectName(const std::string& name);
   std::string GetObjectName() const;

   /// @brief コンポーネントコンテナへの直接アクセス
   ComponentContainer& GetComponentContainer() { return components_; }
   const ComponentContainer& GetComponentContainer() const { return components_; }

#ifdef USE_IMGUI
   /// @brief コンポーネントのインスペクターを描画する
   /// @return 削除ボタンが押されたコンポーネント型名。未選択時は空文字列
   std::string DrawComponentInspector();
#endif

private:
   ComponentContainer components_;
};

} // namespace GameEngine
