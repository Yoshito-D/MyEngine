#include "pch.h"
#include "Object.h"
#include "Component/ComponentRegistry.h"
#include "Component/ObjectNameComponent.h"

namespace GameEngine {

Object::Object() {
   AddComponent<ObjectNameComponent>();
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
   return components_.AddByTypeName(*this, typeName);
}

bool Object::HasComponentByTypeName(const std::string& typeName) const {
   return components_.HasByTypeName(typeName);
}

bool Object::DeserializeComponents(const nlohmann::json& componentsData) {
   return components_.Deserialize(*this, componentsData);
}

nlohmann::json Object::SerializeComponents() const {
   return components_.Serialize();
}

void Object::UpdateComponents(float deltaTime) {
   components_.Update(deltaTime);
}

#ifdef USE_IMGUI
void Object::DrawComponentInspector() {
   components_.DrawInspector();
}
#endif

} // namespace GameEngine
