#pragma once
#pragma once
#include "GraphicsDevice.h"
#include "Mesh.h"
#include "Material.h"
#include "DirectionalLight.h"
#include "TransformationMatrix.h"
#include "Component/IObjectComponent.h"
#include "Component/TransformComponent.h"
#include "Component/MaterialComponent.h"
#include "Component/ColliderComponent.h"
#include "Component/RenderComponent.h"
#include "Component/ObjectNameComponent.h"
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
   Object();
   virtual ~Object() = default;

   /// @brief オブジェクトの初期化
   /// @param device グラフィックスデバイス
   static void Initialize(GraphicsDevice* device);

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

   bool DeserializeComponents(const nlohmann::json& componentsData);

   nlohmann::json SerializeComponents() const;

   void UpdateComponents(float deltaTime);

#ifdef USE_IMGUI
   void DrawComponentInspector();
#endif

   /// @brief トランスフォーメーションマトリックスを取得
   TransformationMatrix* GetTransformationMatrix() { return transformationMatrix_.get(); }

   /// @brief マテリアルのリストを取得する
   /// @return マテリアルのリストへの参照
   const std::vector<Material*>& GetMaterials() const;

   /// @brief マテリアルを設定する
   /// @param material マテリアル（単一マテリアル用）
   void SetMaterial(Material* material);

   /// @brief マテリアルを追加する
   /// @param material 追加するマテリアル
   void AddMaterial(Material* material);

   /// @brief マテリアルを設定する（マルチマテリアル対応）
   /// @param materials マテリアルのリスト
   void SetMaterials(const std::vector<Material*>& materials);

   /// @brief オブジェクトのトランスフォームを設定する
   /// @return トランスフォーム
   Transform GetTransform() const;

   /// @brief 指定インデックスのマテリアルを取得する
   /// @param index マテリアルのインデックス（省略時は0）
   /// @return マテリアルへのポインタ
   Material* GetMaterial(size_t index = 0) const;

   /// @brief マテリアルの数を取得する
   /// @return マテリアルの数
   size_t GetMaterialCount() const;

   /// @brief 親の行列を使用するか設定する
   /// @param isUsing 親のワールド行列を使用する場合はtrue、使用しない場合はfalse
   void SetIsUsingParentMatrix(bool isUsing);

   TransformComponent* GetTransformComponent();
   const TransformComponent* GetTransformComponent() const;

   MaterialComponent* GetMaterialComponent();
   const MaterialComponent* GetMaterialComponent() const;

   ColliderComponent* AddColliderComponent();
   ColliderComponent* GetColliderComponent();
   const ColliderComponent* GetColliderComponent() const;

   RenderComponent* GetRenderComponent();
   const RenderComponent* GetRenderComponent() const;

   ObjectNameComponent* GetObjectNameComponent();
   const ObjectNameComponent* GetObjectNameComponent() const;
   void SetObjectName(const std::string& name);
   std::string GetObjectName() const;

   /// @brief メッシュを取得する
   /// @return メッシュへのポインタ
   Mesh* GetMesh() const { return mesh_.get(); }
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