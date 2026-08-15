#include "pch.h"
#include "Object.h"
#include "Component/ComponentRegistry.h"
#include "Component/ObjectNameComponent.h"
#include "Component/TransformComponent.h"
#include <algorithm>
#include <atomic>
#include <unordered_map>
#include <unordered_set>

namespace {
std::vector<GameEngine::Object*>& RegisteredObjects() {
   // Objectの生成順を保つ一覧。EditorのHierarchy列挙と名前検索に使う。
   static std::vector<GameEngine::Object*> objects;
   return objects;
}

std::unordered_map<std::string, GameEngine::Object*>& RegisteredObjectIds() {
   // Entity IDからの参照解決を毎フレーム線形探索せずに行うための非所有索引。
   static std::unordered_map<std::string, GameEngine::Object*> objectsById;
   return objectsById;
}

std::string AllocateRuntimeEntityId() {
   // 保存前の一時Entityにも一意な参照先を与える。永続化時はEditorが安定IDへ置き換える。
   static std::atomic_uint64_t nextId = 1;
   return "runtime_entity_" + std::to_string(nextId.fetch_add(1));
}

GameEngine::Matrix4x4 CalculateWorldMatrix(
   const GameEngine::Object* object,
   std::unordered_set<const GameEngine::Object*>& visiting) {
   // visitingは現在の再帰経路だけを保持し、壊れたデータに循環があっても無限再帰を防ぐ。
   if (!object || !visiting.insert(object).second) {
      return GameEngine::MakeIdentity4x4();
   }

   GameEngine::Matrix4x4 localMatrix = GameEngine::MakeIdentity4x4();
   if (const auto* transform = object->GetComponent<GameEngine::TransformComponent>()) {
      localMatrix = GameEngine::MakeAffineMatrix(transform->transform);
   }

   const std::string& parentId = object->GetParentEntityId();
   if (parentId.empty()) {
      visiting.erase(object);
      return localMatrix;
   }

   const auto* parent = GameEngine::Object::FindByEntityId(parentId);
   if (!parent || parent == object) {
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

Object::Object()
   : entityId_(AllocateRuntimeEntityId()) {
   RegisteredObjects().push_back(this);
   RegisteredObjectIds()[entityId_] = this;
   // 全Objectがシリアライズ可能な名前を持つよう、生成時に必須コンポーネントを付与する。
   AddComponent<ObjectNameComponent>();
}

Object::~Object() {
   // 子に破棄済みIDを残すと、後から同じIDを再利用したObjectへ意図せず接続されるため解除する。
   for (Object* object : RegisteredObjects()) {
      if (object && object != this && object->GetParentEntityId() == entityId_) {
         object->SetParentEntityId({});
      }
   }
   // RegistryからOwnerを外す前にComponentへDetachを通知し、外部Manager登録を解除させる。
   components_.Clear();
   auto& objectsById = RegisteredObjectIds();
   if (const auto idIt = objectsById.find(entityId_);
      idIt != objectsById.end() && idIt->second == this) {
      objectsById.erase(idIt);
   }
   auto& objects = RegisteredObjects();
   objects.erase(std::remove(objects.begin(), objects.end(), this), objects.end());
}

bool Object::SetEntityId(const std::string& entityId) {
   if (entityId.empty()) {
      return false;
   }
   if (entityId_ == entityId) {
      return true;
   }

   // ID索引の一意性を先に確認し、失敗時は現在のIDと親子関係を一切変更しない。
   auto& objectsById = RegisteredObjectIds();
   if (const auto duplicateIt = objectsById.find(entityId);
      duplicateIt != objectsById.end() && duplicateIt->second != this) {
      return false;
   }

   // 索引を旧→新へ付け替えた後、子が保持する参照もまとめて移行する。
   const std::string previousId = entityId_;
   if (const auto previousIt = objectsById.find(previousId);
      previousIt != objectsById.end() && previousIt->second == this) {
      objectsById.erase(previousIt);
   }
   entityId_ = entityId;
   objectsById[entityId_] = this;
   for (Object* object : RegisteredObjects()) {
      if (object && object != this && object->GetParentEntityId() == previousId) {
         object->SetParentEntityId(entityId_);
      }
   }
   return true;
}

bool Object::SetParentEntityId(const std::string& parentEntityId) {
   // 自己参照だけでなく祖先をたどって循環を拒否し、World行列を常に有限回で計算可能に保つ。
   if (parentEntityId == entityId_ || WouldCreateParentCycle(parentEntityId)) {
      return false;
   }
   parentEntityId_ = parentEntityId;
   return true;
}

Matrix4x4 Object::GetWorldMatrix() const {
   std::unordered_set<const Object*> visiting;
   return CalculateWorldMatrix(this, visiting);
}

Matrix4x4 Object::GetParentWorldMatrix() const {
   if (parentEntityId_.empty()) {
      return MakeIdentity4x4();
   }

   const Object* parent = FindByEntityId(parentEntityId_);
   if (!parent || parent == this) {
      return MakeIdentity4x4();
   }

   // 呼び出し元自身を訪問済みにしておき、既存データに親→子の逆参照があっても打ち切る。
   std::unordered_set<const Object*> visiting;
   visiting.insert(this);
   return CalculateWorldMatrix(parent, visiting);
}

const std::vector<Object*>& Object::GetRegisteredObjects() {
   return RegisteredObjects();
}

Object* Object::FindByEntityId(const std::string& entityId) {
   if (entityId.empty()) {
      return nullptr;
   }
   const auto& objectsById = RegisteredObjectIds();
   const auto it = objectsById.find(entityId);
   return it != objectsById.end() ? it->second : nullptr;
}

Object* Object::FindByObjectName(const std::string& objectName) {
   if (objectName.empty()) {
      return nullptr;
   }
   const auto& objects = RegisteredObjects();
   const auto it = std::find_if(objects.begin(), objects.end(),
      [&objectName](const Object* object) {
         return object && object->GetObjectName() == objectName;
      });
   return it != objects.end() ? *it : nullptr;
}

bool Object::WouldCreateParentCycle(const std::string& parentEntityId) const {
   if (parentEntityId.empty()) {
      return false;
   }

   // 候補親から上へたどり、自分へ到達すれば新しい辺が循環を閉じる。
   // 既に壊れている別系統の循環もvisitedで安全に打ち切る。
   std::unordered_set<const Object*> visited;
   const Object* current = FindByEntityId(parentEntityId);
   while (current && visited.insert(current).second) {
      if (current == this) {
         return true;
      }
      current = FindByEntityId(current->GetParentEntityId());
   }
   return false;
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
	  // 旧データや削除済みコンポーネントでも、ヒエラルキーへ安定した表示名を返す。
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

IObjectComponent* Object::GetComponentByTypeName(const std::string& typeName) const {
   return components_.GetByTypeName(typeName);
}

bool Object::RemoveComponentByTypeName(const std::string& typeName) {
   return components_.RemoveByTypeName(typeName);
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
ComponentInspectorAction Object::DrawComponentInspector(bool canSaveComponent) {
   return components_.DrawInspector(canSaveComponent);
}
#endif

} // namespace GameEngine
