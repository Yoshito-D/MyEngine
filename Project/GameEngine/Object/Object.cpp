#include "pch.h"
#include "Object.h"
#include "Camera.h"
#include "Model/Model.h"
#include "ModelAsset.h"
#include "Component/ComponentRegistry.h"
#include "Component/RenderComponent.h"
#include "Component/AnimationComponent.h"

namespace GameEngine {
namespace {
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;
bool sIsDefaultComponentFactoriesRegistered_ = false;
}

Object::Object() {
   RegisterDefaultComponentFactories();

   auto* transformComponent = AddComponent<TransformComponent>();
   transformComponent->transform.scale = Vector3(1.0f, 1.0f, 1.0f);
   AddComponent<MaterialComponent>();
}

void Object::RegisterDefaultComponentFactories() {
   if (sIsDefaultComponentFactoriesRegistered_) {
	  return;
   }

   auto& registry = ComponentRegistry::GetInstance();
   registry.RegisterFactory("TransformComponent", [](Object& owner) { return owner.AddComponent<TransformComponent>(); });
   registry.RegisterFactory("MaterialComponent", [](Object& owner) { return owner.AddComponent<MaterialComponent>(); });
   registry.RegisterFactory("ColliderComponent", [](Object& owner) { return owner.AddComponent<ColliderComponent>(); });
   registry.RegisterFactory("RenderComponent", [](Object& owner) { return owner.AddComponent<RenderComponent>(); });
   registry.RegisterFactory("ObjectNameComponent", [](Object& owner) { return owner.AddComponent<ObjectNameComponent>(); });
   registry.RegisterFactory("AnimationComponent", [](Object& owner) { return owner.AddComponent<AnimationComponent>(); });

   sIsDefaultComponentFactoriesRegistered_ = true;
}

void Object::Initialize(GraphicsDevice* device) {
   if (sIsInitialized_) return;
   sDevice_ = device;
   sIsInitialized_ = true;
}

Material* Object::GetMaterial(size_t index) const {
   const auto* materialComponent = GetMaterialComponent();
   if (!materialComponent || index >= materialComponent->materials.size()) {
	  return nullptr;
   }
   return materialComponent->materials[index];
}

void Object::SetMaterial(Material* material) {
   assert(material != nullptr);
   auto* materialComponent = GetMaterialComponent();
   assert(materialComponent != nullptr);

   materialComponent->materials.clear();
   materialComponent->materials.push_back(material);
}

void Object::AddMaterial(Material* material) {
   assert(material != nullptr);
   auto* materialComponent = GetMaterialComponent();
   assert(materialComponent != nullptr);

   materialComponent->materials.push_back(material);
}

void Object::SetMaterials(const std::vector<Material*>& materials) {
   assert(!materials.empty());
   // 全てのマテリアルがnullでないことを確認
   for (const auto& material : materials) {
	  assert(material != nullptr);
   }
   auto* materialComponent = GetMaterialComponent();
   assert(materialComponent != nullptr);
   materialComponent->materials = materials;
}

const std::vector<Material*>& Object::GetMaterials() const {
   static const std::vector<Material*> empty;
   const auto* materialComponent = GetMaterialComponent();
   if (!materialComponent) {
	  return empty;
   }
   return materialComponent->materials;
}

Transform Object::GetTransform() const {
   const auto* transformComponent = GetTransformComponent();
   if (!transformComponent) {
	  return Transform();
   }
   return transformComponent->transform;
}

size_t Object::GetMaterialCount() const {
   const auto* materialComponent = GetMaterialComponent();
   if (!materialComponent) {
	  return 0;
   }
   return materialComponent->materials.size();
}

void Object::SetIsUsingParentMatrix(bool isUsing) {
   auto* transformComponent = GetTransformComponent();
   if (!transformComponent) {
	  return;
   }
   transformComponent->useParentMatrix = isUsing;
}

TransformComponent* Object::GetTransformComponent() {
   return GetComponent<TransformComponent>();
}

const TransformComponent* Object::GetTransformComponent() const {
   return GetComponent<TransformComponent>();
}

MaterialComponent* Object::GetMaterialComponent() {
   return GetComponent<MaterialComponent>();
}

const MaterialComponent* Object::GetMaterialComponent() const {
   return GetComponent<MaterialComponent>();
}

ColliderComponent* Object::AddColliderComponent() {
   return AddComponent<ColliderComponent>();
}

ColliderComponent* Object::GetColliderComponent() {
   return GetComponent<ColliderComponent>();
}

const ColliderComponent* Object::GetColliderComponent() const {
   return GetComponent<ColliderComponent>();
}

RenderComponent* Object::GetRenderComponent() {
   return GetComponent<RenderComponent>();
}

const RenderComponent* Object::GetRenderComponent() const {
   return GetComponent<RenderComponent>();
}

ObjectNameComponent* Object::GetObjectNameComponent() {
   return GetComponent<ObjectNameComponent>();
}

const ObjectNameComponent* Object::GetObjectNameComponent() const {
   return GetComponent<ObjectNameComponent>();
}

void Object::SetObjectName(const std::string& name) {
   auto* objectNameComponent = AddComponent<ObjectNameComponent>();
   if (!objectNameComponent) {
	  return;
   }
   objectNameComponent->name = name;
}

std::string Object::GetObjectName() const {
   const auto* objectNameComponent = GetObjectNameComponent();
   if (!objectNameComponent || objectNameComponent->name.empty()) {
	  return "Object";
   }
   return objectNameComponent->name;
}

IObjectComponent* Object::AddComponentByTypeName(const std::string& typeName) {
   if (typeName.empty()) {
	  return nullptr;
   }

   return ComponentRegistry::GetInstance().CreateComponent(*this, typeName);
}

bool Object::DeserializeComponents(const nlohmann::json& componentsData) {
   if (!componentsData.is_array()) {
	  return false;
   }

   bool hasAppliedAnyComponent = false;
   for (const auto& componentData : componentsData) {
	  if (!componentData.is_object()) {
		 continue;
	  }

	  const std::string typeName = componentData.value("typeName", "");
	  if (typeName.empty()) {
		 continue;
	  }

	  auto* component = AddComponentByTypeName(typeName);
	  if (!component) {
		 continue;
	  }

	  if (componentData.contains("enabled") && componentData.at("enabled").is_boolean()) {
		 component->SetEnabled(componentData.at("enabled").get<bool>());
	  }

	  if (componentData.contains("data") && componentData.at("data").is_object()) {
		 component->Deserialize(componentData.at("data"));
	  }

	  hasAppliedAnyComponent = true;
   }

   return hasAppliedAnyComponent;
}

nlohmann::json Object::SerializeComponents() const {
	nlohmann::json componentsData = nlohmann::json::array();

	for (const auto& component : components_) {
	   if (!component) {
		  continue;
	   }

	   nlohmann::json entry = nlohmann::json::object();
	   entry["typeName"] = component->GetTypeName();
	   entry["enabled"] = component->IsEnabled();
	   entry["data"] = component->Serialize();

	   componentsData.push_back(std::move(entry));
	}

	return componentsData;
}

bool Object::RemoveComponentByType(const std::type_index& type) {
   auto mapIt = componentMap_.find(type);
   if (mapIt == componentMap_.end()) {
	  return false;
   }

   IObjectComponent* target = mapIt->second;
   componentMap_.erase(mapIt);

   auto vecIt = std::find_if(components_.begin(), components_.end(),
	  [target](const std::unique_ptr<IObjectComponent>& component) {
		 return component.get() == target;
	  });

   if (vecIt != components_.end()) {
	  components_.erase(vecIt);
	  return true;
   }

   return false;
}

void Object::CreateTransformationMatrix() {
   transformationMatrix_ = std::make_unique<TransformationMatrix>();
   transformationMatrix_->Create();
}

void Object::CreateMesh() {
   if (createMeshFunc_) {
	  createMeshFunc_();
   }
}

void Object::UpdateComponents(float deltaTime) {
   for (auto& component : components_) {
   if (!component || !component->IsEnabled()) continue;
	  component->Update(*this, deltaTime);
   }
}

#ifdef USE_IMGUI
void Object::DrawComponentInspector() {
   for (auto& component : components_) {
   if (!component) continue;
	  component->DrawInspector(*this);
   }
}
#endif
}