#include "pch.h"
#include "Object.h"
#include "Camera.h"
#include "Model/Model.h"
#include "ModelAsset.h"
#include "Component/ComponentRegistry.h"
#include "Component/TransformComponent.h"
#include "Component/MaterialComponent.h"
#include "Component/ColliderComponent.h"
#include "Component/RenderComponent.h"
#include "Component/ObjectNameComponent.h"
#include "Component/AnimationComponent.h"

namespace GameEngine {
namespace {
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;
bool sIsDefaultComponentFactoriesRegistered_ = false;
uint64_t sAutoMaterialCounter_ = 0;

std::string BuildAutoMaterialName() {
   return "ObjectMaterial_" + std::to_string(++sAutoMaterialCounter_);
}
}

Object::Object() {
   RegisterDefaultComponentFactories();

   auto* transformComponent = AddComponent<TransformComponent>();
   transformComponent->transform.scale = Vector3(1.0f, 1.0f, 1.0f);
   auto* materialComponent = AddComponent<MaterialComponent>();
   if (materialComponent) {
	  materialComponent->EnsureMaterial(BuildAutoMaterialName());
   }
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

bool Object::RegisterComponentFactory(const std::string& typeName, ComponentFactory factory) {
   RegisterDefaultComponentFactories();
   return ComponentRegistry::GetInstance().RegisterFactory(typeName, std::move(factory));
}

void Object::SetObjectName(const std::string& name) {
   auto* objectNameComponent = AddComponent<ObjectNameComponent>();
   if (!objectNameComponent) {
	  return;
   }
   objectNameComponent->name = name;
}

std::string Object::GetObjectName() const {
   const auto* objectNameComponent = GetComponent<ObjectNameComponent>();
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

bool Object::HasComponentByTypeName(const std::string& typeName) const {
   if (typeName.empty()) {
	  return false;
   }

   for (const auto& component : components_) {
	  if (!component) {
		 continue;
	  }

	  if (typeName == component->GetTypeName()) {
		 return true;
	  }
   }

   return false;
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