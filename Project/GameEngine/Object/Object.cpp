#include "GameEngine/pch.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Object/Component/Base/ComponentRegistry.h"
#include "GameEngine/Object/Component/Base/ObjectNameComponent.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Component/Rendering/RenderComponent.h"
#include "GameEngine/Scene/SceneWorld.h"
#include <unordered_set>

namespace {
GameEngine::Matrix4x4 CalculateWorldMatrix(
   const GameEngine::Object* object, std::unordered_set<const GameEngine::Object*>& visiting) {
   // visitingは現在の再帰経路だけを保持し、壊れたデータに循環があっても無限再帰を防ぐ。
   if (!object || !visiting.insert(object).second) {
      return GameEngine::MakeIdentity4x4();
   }

   GameEngine::Matrix4x4 localMatrix = GameEngine::MakeIdentity4x4();

   if (const auto* transform = object->GetComponent<GameEngine::TransformComponent>()) {
      localMatrix = GameEngine::MakeAffineMatrix(transform->GetLocalPose());
   }

   const auto* parent = object->GetParent();

   if (!parent) {
      visiting.erase(object);
      return localMatrix;
   }

   // 行ベクトル規約に合わせ、子のLocalを親のWorldへ左から順に連結する。
   const GameEngine::Matrix4x4 worldMatrix = localMatrix * CalculateWorldMatrix(parent, visiting);
   visiting.erase(object);
   return worldMatrix;
}
}

namespace GameEngine {

Object::Object() : components_(*this) {
   // 全Objectがシリアライズ可能な名前を持つよう、生成時に必須コンポーネントを付与する。
   AddComponent<ObjectNameComponent>();
}

Object::~Object() {
   // 部品のDetachはOwnerが生存している間に通知する。
   components_.Clear();
}

bool Object::SetParentEntityId(const std::string& parentEntityId) {
   if (parentEntityId.empty()) {
      parentEntityId_.clear();
      return true;
   }

   const Object* parent = world_ ? world_->FindObjectById(parentEntityId) : nullptr;

   if (!parent) {
      return false;
   }

   // 所属ワールド内で祖先をたどり、新しい辺で循環ができる場合は現在の親を保持する。
   for (auto* ancestor = parent; ancestor; ancestor = ancestor->GetParent()) {
      if (ancestor == this) {
         return false;
      }
   }

   parentEntityId_ = parentEntityId;
   return true;
}

const Object* Object::GetParent() const {
   return world_ && !parentEntityId_.empty() ? world_->FindObjectById(parentEntityId_) : nullptr;
}

Matrix4x4 Object::GetWorldMatrix() const {
   std::unordered_set<const Object*> visiting;
   return CalculateWorldMatrix(this, visiting);
}

Quaternion Object::GetWorldRotation() const {
   auto rotation = Quaternion::Identity();
   for (auto* object = this; object; object = object->GetParent()) {
      if (const auto* transform = object->GetComponent<TransformComponent>()) {
         rotation = transform->GetLocalPose().GetActiveQuaternion() * rotation;
      }
   }
   return rotation.Normalize();
}

Matrix4x4 Object::GetParentWorldMatrix() const {
   const auto* parent = GetParent();

   if (!parent) {
      return MakeIdentity4x4();
   }

   // 呼び出し元自身を訪問済みにしておき、既存データに親→子の逆参照があっても打ち切る。
   std::unordered_set<const Object*> visiting;
   visiting.insert(this);
   return CalculateWorldMatrix(parent, visiting);
}

void Object::SetObjectName(const std::string& name) {
   auto* objectNameComponent = AddComponent<ObjectNameComponent>();

   if (!objectNameComponent) return;

   {
      auto settings = objectNameComponent->DescribeSettings();
      settings.name = name;
      objectNameComponent->Configure(settings);
   }
}

std::string Object::GetObjectName() const {
   const auto* objectNameComponent = GetComponent<ObjectNameComponent>();

   if (!objectNameComponent || objectNameComponent->DescribeSettings().name.empty()) {
      // 旧データや削除済みコンポーネントでも、ヒエラルキーへ安定した表示名を返す。
      return "Object";
   }

   return objectNameComponent->DescribeSettings().name;
}

IObjectComponent* Object::AddComponentByTypeName(const std::string& typeName) {
   return components_.AddByTypeName(typeName);
}

bool Object::HasComponentByTypeName(const std::string& typeName) const {
   return components_.HasByTypeName(typeName);
}

IObjectComponent* Object::GetComponentByTypeName(const std::string& typeName) {
   return components_.GetByTypeName(typeName);
}

const IObjectComponent* Object::GetComponentByTypeName(const std::string& typeName) const {
   return components_.GetByTypeName(typeName);
}

bool Object::RemoveComponentByTypeName(const std::string& typeName) {
   return components_.RemoveByTypeName(typeName);
}

nlohmann::json Object::SerializeObjectData() const {
   return nlohmann::json::object();
}

void Object::DeserializeObjectData(const nlohmann::json& objectData) {
   (void)objectData;
}

bool Object::DeserializeComponents(const nlohmann::json& componentsData) {
   return components_.Deserialize(componentsData);
}

nlohmann::json Object::SerializeComponents() const {
   return components_.Serialize();
}

void Object::PrepareDraw(const SceneDrawContext& context) {
   components_.PrepareDraw(context);
}

void Object::UpdateComponents(float deltaTime, ObjectUpdateDomain domain, std::optional<ObjectUpdatePhase> phase) {
   components_.Update(deltaTime, domain, phase);
}

void Object::Activate() {
   components_.Activate();

   if (auto* renderComponent = GetComponent<RenderComponent>()) {
      {
         auto settings = renderComponent->DescribeSettings();
         settings.visible = true;
         renderComponent->Configure(settings);
      }
   }
}

void Object::Deactivate() {
   components_.Deactivate();

   if (auto* renderComponent = GetComponent<RenderComponent>()) {
      {
         auto settings = renderComponent->DescribeSettings();
         settings.visible = false;
         renderComponent->Configure(settings);
      }
   }
}

void Object::ResolveComponentReferences(SceneWorld& sceneWorld, bool initializeRuntime) {
   components_.ResolveReferences(sceneWorld, initializeRuntime);
}

#ifdef USE_IMGUI
ComponentInspectorAction Object::DrawComponentInspector(bool canSaveComponent) {
   return components_.DrawInspector(canSaveComponent);
}
#endif

} // namespace GameEngine
