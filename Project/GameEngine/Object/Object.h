#pragma once
#pragma once
#include "GraphicsDevice.h"
#include "Mesh.h"
#include "DirectionalLight.h"
#include "TransformationMatrix.h"
#include "Component/IObjectComponent.h"
#include "Utility/Math/Transform.h"
#include <memory>
#include <functional>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <algorithm>
#include <utility>
#include <vector>

namespace GameEngine {
class Camera;

class Object {
public:
   using ComponentFactory = std::function<IObjectComponent*(Object&)>;

   Object();
   virtual ~Object() = default;

   /// @brief オブジェクトの初期化
   /// @param device グラフィックスデバイス
   static void Initialize(GraphicsDevice* device);

   static bool RegisterComponentFactory(const std::string& typeName, ComponentFactory factory);

   template <typename T>
   static bool RegisterComponentType(const std::string& typeName) {
	  static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");
	  return RegisterComponentFactory(typeName, [](Object& owner) {
		 return owner.AddComponent<T>();
	  });
   }

   template <typename T, typename... Args>
   T* AddComponent(Args&&... args) {
     static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");

	  const std::type_index type = std::type_index(typeid(T));
	  auto it = componentMap_.find(type);
	  if (it != componentMap_.end()) {
		 return static_cast<T*>(it->second);
	  }

	  auto component = std::make_unique<T>(std::forward<Args>(args)...);
	  T* rawPtr = component.get();
	  components_.push_back(std::move(component));
	  componentMap_[type] = rawPtr;
	  return rawPtr;
   }

   template <typename T>
   T* GetComponent() {
     static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");

	  const std::type_index type = std::type_index(typeid(T));
	  auto it = componentMap_.find(type);
	  if (it == componentMap_.end()) {
		 return nullptr;
	  }
	  return static_cast<T*>(it->second);
   }

   template <typename T>
   const T* GetComponent() const {
     static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");

	  const std::type_index type = std::type_index(typeid(T));
	  auto it = componentMap_.find(type);
	  if (it == componentMap_.end()) {
		 return nullptr;
	  }
	  return static_cast<const T*>(it->second);
   }

   template <typename T>
   bool HasComponent() const {
     static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");
	  return componentMap_.contains(std::type_index(typeid(T)));
   }

   template <typename T>
   bool RemoveComponent() {
	  static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");

	  const std::type_index type = std::type_index(typeid(T));
	  return RemoveComponentByType(type);
   }

   template <typename T>
   bool SetComponentEnabled(bool enabled) {
	  static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");

	  auto* component = GetComponent<T>();
	  if (!component) {
		 return false;
	  }

	  component->SetEnabled(enabled);
	  return true;
   }

   template <typename T>
   bool IsComponentEnabled() const {
	  static_assert(std::is_base_of_v<IObjectComponent, T>, "T must derive from IObjectComponent");

	  const auto* component = GetComponent<T>();
	  if (!component) {
		 return false;
	  }

	  return component->IsEnabled();
   }

   IObjectComponent* AddComponentByTypeName(const std::string& typeName);
   bool HasComponentByTypeName(const std::string& typeName) const;

   bool DeserializeComponents(const nlohmann::json& componentsData);

   nlohmann::json SerializeComponents() const;

   void UpdateComponents(float deltaTime);

#ifdef USE_IMGUI
   void DrawComponentInspector();
#endif

   /// @brief トランスフォーメーションマトリックスを取得
   TransformationMatrix* GetTransformationMatrix() { return transformationMatrix_.get(); }

   void SetObjectName(const std::string& name);
   std::string GetObjectName() const;
protected:
   enum class MeshType {
	  Sprite,
   };

   std::unique_ptr<Mesh> mesh_ = nullptr;
   std::unique_ptr<TransformationMatrix> transformationMatrix_ = nullptr;

protected:

   void CreateMesh();

   void CreateTransformationMatrix();

   void SetCreateMeshFunction(std::function<void()> func) { createMeshFunc_ = func; }
private:
   static void RegisterDefaultComponentFactories();

   bool RemoveComponentByType(const std::type_index& type);

   std::function<void()> createMeshFunc_;
   std::vector<std::unique_ptr<IObjectComponent>> components_;
   std::unordered_map<std::type_index, IObjectComponent*> componentMap_;
};
}