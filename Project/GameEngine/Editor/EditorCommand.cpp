#include "pch.h"
#include "EditorCommand.h"

#ifdef USE_IMGUI

#include "Component/TransformComponent.h"
#include "EditorSceneContext.h"
#include "Object.h"

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
}

void EditorCommandStack::Redo(EditorSceneContext& context) {
   if (redoStack_.empty()) {
      return;
   }

   auto command = std::move(redoStack_.back());
   redoStack_.pop_back();
   if (command->Execute(context)) {
      undoStack_.push_back(std::move(command));
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

} // namespace GameEngine

#endif
