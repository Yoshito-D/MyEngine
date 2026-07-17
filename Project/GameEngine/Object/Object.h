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
   void DrawComponentInspector();
#endif

private:
   ComponentContainer components_;
};

} // namespace GameEngine
