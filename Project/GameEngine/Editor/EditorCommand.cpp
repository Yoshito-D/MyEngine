#include "pch.h"
#include "EditorCommand.h"

#ifdef USE_IMGUI

#include "Component/MaterialComponent.h"
#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"
#include "EditorSceneContext.h"
#include "Effect/ParticleSystem.h"
#include "Framework/EngineContext.h"
#include "Object.h"
#include <filesystem>

namespace GameEngine {

bool EditorCommandStack::Execute(std::unique_ptr<IEditorCommand> command, EditorSceneContext& context) {
   if (!command) {
      return false;
   }

   if (!command->Execute(context)) {
      return false;
   }

   undoStack_.push_back(std::move(command));
   redoStack_.clear();
   context.MarkDirty();
   return true;
}

void EditorCommandStack::Undo(EditorSceneContext& context) {
   if (undoStack_.empty()) {
      return;
   }

   auto command = std::move(undoStack_.back());
   undoStack_.pop_back();
   command->Undo(context);
   redoStack_.push_back(std::move(command));
   context.MarkDirty();
}

void EditorCommandStack::Redo(EditorSceneContext& context) {
   if (redoStack_.empty()) {
      return;
   }

   auto command = std::move(redoStack_.back());
   redoStack_.pop_back();
   if (command->Execute(context)) {
      undoStack_.push_back(std::move(command));
      context.MarkDirty();
   }
}

void EditorCommandStack::Clear() {
   undoStack_.clear();
   redoStack_.clear();
}

const char* EditorCommandStack::GetUndoName() const {
   return undoStack_.empty() ? "" : undoStack_.back()->GetName();
}

const char* EditorCommandStack::GetRedoName() const {
   return redoStack_.empty() ? "" : redoStack_.back()->GetName();
}

CreateGenericObjectCommand::CreateGenericObjectCommand(Transform initialTransform)
   : initialTransform_(initialTransform) {
}

bool CreateGenericObjectCommand::Execute(EditorSceneContext& context) {
   Object* object = nullptr;
   if (!snapshot_.is_null() && snapshot_.is_object()) {
      object = context.GetObjectStore().RestoreObject(snapshot_);
   } else {
      object = context.GetObjectStore().CreateGenericObject(&initialTransform_);
   }

   if (!object) {
      return false;
   }

   objectId_ = context.GetObjectStore().GetId(object);
   context.SelectObject(object);
   return true;
}

void CreateGenericObjectCommand::Undo(EditorSceneContext& context) {
   if (objectId_.empty()) {
      return;
   }

   snapshot_ = context.GetObjectStore().SerializeObject(objectId_);
   if (context.GetSelectedObject() == context.GetObjectStore().FindById(objectId_)) {
      context.SelectObject(nullptr);
   }
   context.GetObjectStore().DeleteObject(objectId_);
}

CreateModelCommand::CreateModelCommand(std::string assetId, Transform initialTransform)
   : assetId_(std::move(assetId))
   , initialTransform_(initialTransform) {
}

bool CreateModelCommand::Execute(EditorSceneContext& context) {
   Object* object = nullptr;
   if (!snapshot_.is_null() && snapshot_.is_object()) {
      object = context.GetObjectStore().RestoreObject(snapshot_);
   } else {
      object = context.GetObjectStore().CreateModel(assetId_, &initialTransform_);
   }

   if (!object) {
      return false;
   }

   objectId_ = context.GetObjectStore().GetId(object);
   context.SelectObject(object);
   return true;
}

void CreateModelCommand::Undo(EditorSceneContext& context) {
   if (objectId_.empty()) {
      return;
   }

   snapshot_ = context.GetObjectStore().SerializeObject(objectId_);
   if (context.GetSelectedObject() == context.GetObjectStore().FindById(objectId_)) {
      context.SelectObject(nullptr);
   }
   context.GetObjectStore().DeleteObject(objectId_);
}

CreateSpriteCommand::CreateSpriteCommand(std::string textureAssetId, Transform initialTransform)
   : textureAssetId_(std::move(textureAssetId))
   , initialTransform_(initialTransform) {
}

bool CreateSpriteCommand::Execute(EditorSceneContext& context) {
   Object* object = nullptr;
   if (!snapshot_.is_null() && snapshot_.is_object()) {
      object = context.GetObjectStore().RestoreObject(snapshot_);
   } else {
      object = context.GetObjectStore().CreateSprite(textureAssetId_, &initialTransform_);
   }

   if (!object) {
      return false;
   }

   objectId_ = context.GetObjectStore().GetId(object);
   context.SelectObject(object);
   return true;
}

void CreateSpriteCommand::Undo(EditorSceneContext& context) {
   if (objectId_.empty()) {
      return;
   }

   snapshot_ = context.GetObjectStore().SerializeObject(objectId_);
   if (context.GetSelectedObject() == context.GetObjectStore().FindById(objectId_)) {
      context.SelectObject(nullptr);
   }
   context.GetObjectStore().DeleteObject(objectId_);
}

CreateUITextCommand::CreateUITextCommand(Transform initialTransform)
   : initialTransform_(initialTransform) {
}

bool CreateUITextCommand::Execute(EditorSceneContext& context) {
   Object* object = nullptr;
   if (!snapshot_.is_null() && snapshot_.is_object()) {
      object = context.GetObjectStore().RestoreObject(snapshot_);
   } else {
      object = context.GetObjectStore().CreateUIText(&initialTransform_);
   }

   if (!object) {
      return false;
   }

   objectId_ = context.GetObjectStore().GetId(object);
   context.SelectObject(object);
   return true;
}

void CreateUITextCommand::Undo(EditorSceneContext& context) {
   if (objectId_.empty()) {
      return;
   }

   snapshot_ = context.GetObjectStore().SerializeObject(objectId_);
   if (context.GetSelectedObject() == context.GetObjectStore().FindById(objectId_)) {
      context.SelectObject(nullptr);
   }
   context.GetObjectStore().DeleteObject(objectId_);
}

CreateParticleSystemCommand::CreateParticleSystemCommand(std::string assetId, Transform initialTransform)
   : assetId_(std::move(assetId))
   , initialTransform_(initialTransform) {
}

bool CreateParticleSystemCommand::Execute(EditorSceneContext& context) {
   ParticleSystem* particleSystem = nullptr;
   if (!snapshot_.is_null() && snapshot_.is_object()) {
      particleSystem = context.GetObjectStore().RestoreParticleSystem(snapshot_);
   } else {
      particleSystem = context.GetObjectStore().CreateParticleSystem(assetId_, {}, &initialTransform_);
   }

   if (!particleSystem) {
      return false;
   }

   objectId_ = context.GetObjectStore().GetId(particleSystem);
   context.SelectObject(nullptr);
   context.SelectParticleSystem(particleSystem);
   return true;
}

void CreateParticleSystemCommand::Undo(EditorSceneContext& context) {
   if (objectId_.empty()) {
      return;
   }

   snapshot_ = context.GetObjectStore().SerializeObject(objectId_);
   if (context.GetSelectedParticleSystem() == context.GetObjectStore().FindParticleById(objectId_)) {
      context.SelectParticleSystem(nullptr);
   }
   context.GetObjectStore().DeleteParticleSystem(objectId_);
}

DeleteObjectCommand::DeleteObjectCommand(std::string objectId)
   : objectId_(std::move(objectId)) {
}

bool DeleteObjectCommand::Execute(EditorSceneContext& context) {
   if (objectId_.empty() || !context.GetObjectStore().ContainsId(objectId_)) {
      return false;
   }

   Object* object = context.GetObjectStore().FindById(objectId_);
   snapshot_ = context.GetObjectStore().SerializeObject(objectId_);
   if (context.GetSelectedObject() == object) {
      context.SelectObject(nullptr);
   }
   return context.GetObjectStore().DeleteObject(objectId_);
}

void DeleteObjectCommand::Undo(EditorSceneContext& context) {
   if (snapshot_.is_null() || !snapshot_.is_object()) {
      return;
   }

   Object* object = context.GetObjectStore().RestoreObject(snapshot_);
   if (object) {
      context.SelectObject(object);
   }
}

DeleteParticleSystemCommand::DeleteParticleSystemCommand(std::string objectId)
   : objectId_(std::move(objectId)) {
}

bool DeleteParticleSystemCommand::Execute(EditorSceneContext& context) {
   if (objectId_.empty() || !context.GetObjectStore().ContainsId(objectId_)) {
      return false;
   }

   ParticleSystem* particleSystem = context.GetObjectStore().FindParticleById(objectId_);
   if (!particleSystem) {
      return false;
   }

   snapshot_ = context.GetObjectStore().SerializeObject(objectId_);
   if (context.GetSelectedParticleSystem() == particleSystem) {
      context.SelectParticleSystem(nullptr);
   }
   return context.GetObjectStore().DeleteParticleSystem(objectId_);
}

void DeleteParticleSystemCommand::Undo(EditorSceneContext& context) {
   if (snapshot_.is_null() || !snapshot_.is_object()) {
      return;
   }

   ParticleSystem* particleSystem = context.GetObjectStore().RestoreParticleSystem(snapshot_);
   if (particleSystem) {
      context.SelectObject(nullptr);
      context.SelectParticleSystem(particleSystem);
   }
}

TransformObjectCommand::TransformObjectCommand(std::string objectId, Object* fallbackObject, const Transform& before, const Transform& after)
   : objectId_(std::move(objectId))
   , fallbackObject_(fallbackObject)
   , before_(before)
   , after_(after) {
}

bool TransformObjectCommand::Execute(EditorSceneContext& context) {
   Object* object = ResolveObject(context);
   if (!object) {
      return false;
   }
   Apply(context, after_);
   return true;
}

void TransformObjectCommand::Undo(EditorSceneContext& context) {
   Apply(context, before_);
}

Object* TransformObjectCommand::ResolveObject(EditorSceneContext& context) const {
   if (!objectId_.empty()) {
      if (Object* object = context.GetObjectStore().FindById(objectId_)) {
         return object;
      }
   }
   return fallbackObject_;
}

void TransformObjectCommand::Apply(EditorSceneContext& context, const Transform& transform) const {
   Object* object = ResolveObject(context);
   if (!object) {
      return;
   }

   auto* transformComponent = object->GetComponent<TransformComponent>();
   if (!transformComponent) {
      return;
   }

   transformComponent->transform = transform;
}

TransformParticleSystemCommand::TransformParticleSystemCommand(std::string objectId, ParticleSystem* fallbackParticleSystem, const Transform& before, const Transform& after)
   : objectId_(std::move(objectId))
   , fallbackParticleSystem_(fallbackParticleSystem)
   , before_(before)
   , after_(after) {
}

bool TransformParticleSystemCommand::Execute(EditorSceneContext& context) {
   ParticleSystem* particleSystem = ResolveParticleSystem(context);
   if (!particleSystem) {
      return false;
   }
   Apply(context, after_);
   return true;
}

void TransformParticleSystemCommand::Undo(EditorSceneContext& context) {
   Apply(context, before_);
}

ParticleSystem* TransformParticleSystemCommand::ResolveParticleSystem(EditorSceneContext& context) const {
   if (!objectId_.empty()) {
      if (ParticleSystem* particleSystem = context.GetObjectStore().FindParticleById(objectId_)) {
         return particleSystem;
      }
   }
   return fallbackParticleSystem_;
}

void TransformParticleSystemCommand::Apply(EditorSceneContext& context, const Transform& transform) const {
   ParticleSystem* particleSystem = ResolveParticleSystem(context);
   if (!particleSystem || !particleSystem->GetShapeModule()) {
      return;
   }

   particleSystem->GetShapeModule()->SetTransform(transform);
}

RestoreObjectSnapshotCommand::RestoreObjectSnapshotCommand(nlohmann::json snapshot, std::string commandName)
   : snapshot_(std::move(snapshot))
   , commandName_(std::move(commandName)) {
}

bool RestoreObjectSnapshotCommand::Execute(EditorSceneContext& context) {
   if (snapshot_.is_null() || !snapshot_.is_object()) {
      return false;
   }

   const std::string objectType = snapshot_.value("objectType", "Model");
   if (objectType == "ParticleSystem") {
      ParticleSystem* particleSystem = context.GetObjectStore().RestoreParticleSystem(snapshot_);
      if (!particleSystem) {
         return false;
      }
      restoredObjectId_ = context.GetObjectStore().GetId(particleSystem);
      context.SelectObject(nullptr);
      context.SelectParticleSystem(particleSystem);
      return true;
   }

   Object* object = context.GetObjectStore().RestoreObject(snapshot_);
   if (!object) {
      return false;
   }

   restoredObjectId_ = context.GetObjectStore().GetId(object);
   context.SelectParticleSystem(nullptr);
   context.SelectObject(object);
   return true;
}

void RestoreObjectSnapshotCommand::Undo(EditorSceneContext& context) {
   if (restoredObjectId_.empty()) {
      return;
   }

   if (ParticleSystem* particleSystem = context.GetObjectStore().FindParticleById(restoredObjectId_)) {
      if (context.GetSelectedParticleSystem() == particleSystem) {
         context.SelectParticleSystem(nullptr);
      }
      context.GetObjectStore().DeleteParticleSystem(restoredObjectId_);
      return;
   }

   if (Object* object = context.GetObjectStore().FindById(restoredObjectId_)) {
      if (context.GetSelectedObject() == object) {
         context.SelectObject(nullptr);
      }
      context.GetObjectStore().DeleteObject(restoredObjectId_);
   }
}

SetModelAssetCommand::SetModelAssetCommand(std::string objectId, Object* fallbackObject, std::string beforeAssetId, std::string afterAssetId)
   : objectId_(std::move(objectId))
   , fallbackObject_(fallbackObject)
   , beforeAssetId_(std::move(beforeAssetId))
   , afterAssetId_(std::move(afterAssetId)) {
}

bool SetModelAssetCommand::Execute(EditorSceneContext& context) {
   return Apply(context, afterAssetId_);
}

void SetModelAssetCommand::Undo(EditorSceneContext& context) {
   Apply(context, beforeAssetId_);
}

Object* SetModelAssetCommand::ResolveObject(EditorSceneContext& context) const {
   if (!objectId_.empty()) {
      if (Object* object = context.GetObjectStore().FindById(objectId_)) {
         return object;
      }
   }
   return fallbackObject_;
}

bool SetModelAssetCommand::Apply(EditorSceneContext& context, const std::string& assetId) const {
   Object* object = ResolveObject(context);
   if (!object) {
      return false;
   }

   auto* meshComponent = object->GetComponent<MeshComponent>();
   if (!meshComponent) {
      return false;
   }

   return meshComponent->SetModelAssetByAssetId(assetId);
}

SetMaterialTextureCommand::SetMaterialTextureCommand(std::string objectId, Object* fallbackObject, size_t slot, std::string beforeTextureId, std::string afterTextureId)
   : objectId_(std::move(objectId))
   , fallbackObject_(fallbackObject)
   , slot_(slot)
   , beforeTextureId_(std::move(beforeTextureId))
   , afterTextureId_(std::move(afterTextureId)) {
}

bool SetMaterialTextureCommand::Execute(EditorSceneContext& context) {
   return Apply(context, afterTextureId_);
}

void SetMaterialTextureCommand::Undo(EditorSceneContext& context) {
   Apply(context, beforeTextureId_);
}

Object* SetMaterialTextureCommand::ResolveObject(EditorSceneContext& context) const {
   if (!objectId_.empty()) {
      if (Object* object = context.GetObjectStore().FindById(objectId_)) {
         return object;
      }
   }
   return fallbackObject_;
}

bool SetMaterialTextureCommand::Apply(EditorSceneContext& context, const std::string& textureId) const {
   Object* object = ResolveObject(context);
   if (!object) {
      return false;
   }

   auto* materialComponent = object->GetComponent<MaterialComponent>();
   if (!materialComponent) {
      return false;
   }

   materialComponent->SetTextureName(slot_, textureId);
   return true;
}

AddComponentCommand::AddComponentCommand(std::string objectId, Object* fallbackObject, std::string typeName)
   : objectId_(std::move(objectId))
   , fallbackObject_(fallbackObject)
   , typeName_(std::move(typeName)) {
}

bool AddComponentCommand::Execute(EditorSceneContext& context) {
   Object* object = ResolveObject(context);
   if (!object || typeName_.empty() || object->HasComponentByTypeName(typeName_)) {
      return false;
   }

   if (beforeSnapshot_.is_null() && !objectId_.empty()) {
      beforeSnapshot_ = context.GetObjectStore().SerializeObject(objectId_);
   }

   return object->AddComponentByTypeName(typeName_) != nullptr;
}

void AddComponentCommand::Undo(EditorSceneContext& context) {
   if (objectId_.empty() || beforeSnapshot_.is_null() || !beforeSnapshot_.is_object()) {
      return;
   }

   Object* selectedObject = context.GetSelectedObject();
   if (selectedObject == context.GetObjectStore().FindById(objectId_)) {
      context.SelectObject(nullptr);
   }
   context.GetObjectStore().DeleteObject(objectId_);
   Object* restored = context.GetObjectStore().RestoreObject(beforeSnapshot_);
   if (restored) {
      context.SelectObject(restored);
   }
}

Object* AddComponentCommand::ResolveObject(EditorSceneContext& context) const {
   if (!objectId_.empty()) {
      if (Object* object = context.GetObjectStore().FindById(objectId_)) {
         return object;
      }
   }
   return fallbackObject_;
}

RemoveComponentCommand::RemoveComponentCommand(
   std::string objectId,
   Object* fallbackObject,
   std::string typeName
)
   : objectId_(std::move(objectId))
   , fallbackObject_(fallbackObject)
   , typeName_(std::move(typeName)) {
}

bool RemoveComponentCommand::Execute(EditorSceneContext& context) {
   Object* object = ResolveObject(context);
   if (!object || typeName_.empty()) {
      return false;
   }

   IObjectComponent* component = object->GetComponentByTypeName(typeName_);
   if (!component) {
      return false;
   }

   if (removedComponentData_.is_null()) {
      removedComponentData_ = nlohmann::json{
         { "enabled", component->IsEnabled() },
         { "data", component->Serialize() }
      };
   }
   return object->RemoveComponentByTypeName(typeName_);
}

void RemoveComponentCommand::Undo(EditorSceneContext& context) {
   Object* object = ResolveObject(context);
   if (!object || removedComponentData_.is_null()) {
      return;
   }

   IObjectComponent* component = object->AddComponentByTypeName(typeName_);
   if (!component) {
      return;
   }
   if (removedComponentData_.contains("enabled") && removedComponentData_.at("enabled").is_boolean()) {
      component->SetEnabled(removedComponentData_.at("enabled").get<bool>());
   }
   if (removedComponentData_.contains("data") && removedComponentData_.at("data").is_object()) {
      component->Deserialize(removedComponentData_.at("data"));
   }
}

Object* RemoveComponentCommand::ResolveObject(EditorSceneContext& context) const {
   if (!objectId_.empty()) {
      if (Object* object = context.GetObjectStore().FindById(objectId_)) {
         return object;
      }
   }
   return fallbackObject_;
}

} // namespace GameEngine

#endif
