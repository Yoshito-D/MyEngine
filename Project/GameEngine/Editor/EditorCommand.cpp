#include "GameEngine/pch.h"
#include "GameEngine/Editor/EditorCommand.h"

#ifdef USE_IMGUI

namespace GameEngine {

bool EditorCommandStack::Execute(std::unique_ptr<IEditorCommand> command, EditorCommandTarget& context) {
   if (!command || !command->Execute(context)) {
      return false;
   }

   // 新しい分岐を実行した時点で以前のRedo履歴は到達不能になるため破棄する。
   undoStack_.push_back(std::move(command));
   redoStack_.clear();
   return true;
}

bool EditorCommandStack::Undo(EditorCommandTarget& context) {
   if (undoStack_.empty() || !undoStack_.back()->Undo(context)) {
      return false;
   }

   // 復元に成功した同じCommandを移し、Undo中に更新したRedo用スナップショットを保持する。
   redoStack_.push_back(std::move(undoStack_.back()));
   undoStack_.pop_back();
   return true;
}

bool EditorCommandStack::Redo(EditorCommandTarget& context) {
   if (redoStack_.empty() || !redoStack_.back()->Execute(context)) {
      return false;
   }

   undoStack_.push_back(std::move(redoStack_.back()));
   redoStack_.pop_back();
   return true;
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

CreateObjectCommand::CreateObjectCommand(Kind kind, Transform transform, std::string assetId, std::string parentId)
   : kind_(kind), initialTransform_(transform), assetId_(std::move(assetId)), parentId_(std::move(parentId)) {
}

const char* CreateObjectCommand::GetName() const {
   switch (kind_) {
      case Kind::Model:
         return "Create Model";
      case Kind::Sprite:
         return "Create Sprite";
      case Kind::UIText:
         return "Create UI Text";
      case Kind::Skybox:
         return "Create Skybox";
      case Kind::Particle:
         return "Create Particle System";
      case Kind::DirectionalLight:
         return "Create Directional Light";
      case Kind::PointLight:
         return "Create Point Light";
      case Kind::SpotLight:
         return "Create Spot Light";
      case Kind::AreaLight:
         return "Create Area Light";
      default:
         return "Create Empty Object";
   }
}

bool CreateObjectCommand::Execute(EditorCommandTarget& context) {
   auto restored = context.Create(kind_, initialTransform_, assetId_, parentId_, snapshot_);

   if (!restored.is_object() || restored.value("id", "").empty()) {
      return false;
   }

   // 初回と再実行で同じ登録IDと保存状態を保持する。
   objectId_ = restored.value("id", "");
   snapshot_ = std::move(restored);
   return true;
}

bool CreateObjectCommand::Undo(EditorCommandTarget& context) {
   const bool particle = kind_ == Kind::Particle;
   RemovedEditorEntity removed;

   if (!context.Remove(objectId_, particle, removed)) {
      return false;
   }

   // Undo前の全状態を保持し、Redoで同じID・設定・親を復元する。
   snapshot_ = std::move(removed.snapshot);
   return true;
}

EditObjectStateCommand::EditObjectStateCommand(
   std::string id, nlohmann::json before, nlohmann::json after, bool particle, std::string name)
   : id_(std::move(id)), name_(std::move(name)), before_(std::move(before)), after_(std::move(after)),
     particle_(particle) {
}

bool EditObjectStateCommand::Execute(EditorCommandTarget& context) {
   if (before_ == after_) {
      return false;
   }

   if (alreadyApplied_) {
      if (!context.Contains(id_, particle_)) {
         return false;
      }

      alreadyApplied_ = false;
      return true;
   }

   return Apply(context, after_);
}

bool EditObjectStateCommand::Undo(EditorCommandTarget& context) {
   return Apply(context, before_);
}

bool EditObjectStateCommand::Apply(EditorCommandTarget& context, const nlohmann::json& snapshot) {
   return context.ApplySnapshot(id_, snapshot, particle_);
}

EditVirtualCameraStateCommand::EditVirtualCameraStateCommand(
   std::string id, nlohmann::json before, nlohmann::json after)
   : id_(std::move(id)), before_(std::move(before)), after_(std::move(after)) {
}

bool EditVirtualCameraStateCommand::Execute(EditorCommandTarget& context) {
   if (id_.empty() || !before_.is_object() || !after_.is_object() || before_ == after_ || !before_.contains("id") ||
       !before_.at("id").is_string() || !after_.contains("id") || !after_.at("id").is_string() ||
       before_.at("id").get<std::string>() != id_ || after_.at("id").get<std::string>() != id_) {
      return false;
   }

   if (alreadyApplied_) {
      if (!context.ContainsCamera(id_)) {
         return false;
      }

      alreadyApplied_ = false;
      return true;
   }

   return context.ApplyCameraSnapshot(id_, after_);
}

bool EditVirtualCameraStateCommand::Undo(EditorCommandTarget& context) {
   return context.ApplyCameraSnapshot(id_, before_);
}

ReorderObjectCommand::ReorderObjectCommand(std::string id, std::string beforeParent, std::string afterParent,
   std::vector<std::string> beforeOrder, std::vector<std::string> afterOrder)
   : id_(std::move(id)), beforeParent_(std::move(beforeParent)), afterParent_(std::move(afterParent)),
     beforeOrder_(std::move(beforeOrder)), afterOrder_(std::move(afterOrder)) {
}

bool ReorderObjectCommand::Execute(EditorCommandTarget& context) {
   if (beforeParent_ == afterParent_ && beforeOrder_ == afterOrder_) {
      return false;
   }

   return Apply(context, afterParent_, afterOrder_);
}

bool ReorderObjectCommand::Undo(EditorCommandTarget& context) {
   return Apply(context, beforeParent_, beforeOrder_);
}

bool ReorderObjectCommand::Apply(
   EditorCommandTarget& context, const std::string& parent, const std::vector<std::string>& order) {
   return context.Reorder(id_, parent, order);
}

DeleteObjectCommand::DeleteObjectCommand(std::string objectId) : objectId_(std::move(objectId)) {
}

bool DeleteObjectCommand::Execute(EditorCommandTarget& context) {
   return context.Remove(objectId_, false, removed_);
}

bool DeleteObjectCommand::Undo(EditorCommandTarget& context) {
   return context.Restore(removed_, false);
}

DeleteParticleSystemCommand::DeleteParticleSystemCommand(std::string objectId) : objectId_(std::move(objectId)) {
}

bool DeleteParticleSystemCommand::Execute(EditorCommandTarget& context) {
   return context.Remove(objectId_, true, removed_);
}

bool DeleteParticleSystemCommand::Undo(EditorCommandTarget& context) {
   return context.Restore(removed_, true);
}

TransformObjectCommand::TransformObjectCommand(std::string objectId, const Transform& before, const Transform& after)
   : objectId_(std::move(objectId)), before_(before), after_(after) {
}

bool TransformObjectCommand::Execute(EditorCommandTarget& context) {
   return context.ApplyPose(objectId_, after_, false);
}

bool TransformObjectCommand::Undo(EditorCommandTarget& context) {
   return context.ApplyPose(objectId_, before_, false);
}

TransformParticleSystemCommand::TransformParticleSystemCommand(
   std::string objectId, const Transform& before, const Transform& after)
   : objectId_(std::move(objectId)), before_(before), after_(after) {
}

bool TransformParticleSystemCommand::Execute(EditorCommandTarget& context) {
   return context.ApplyPose(objectId_, after_, true);
}

bool TransformParticleSystemCommand::Undo(EditorCommandTarget& context) {
   return context.ApplyPose(objectId_, before_, true);
}

RestoreObjectSnapshotCommand::RestoreObjectSnapshotCommand(nlohmann::json snapshot, std::string commandName)
   : snapshot_(std::move(snapshot)), commandName_(std::move(commandName)) {
}

bool RestoreObjectSnapshotCommand::Execute(EditorCommandTarget& context) {
   if (!snapshot_.is_object()) {
      return false;
   }

   auto restored = context.RestoreSnapshot(snapshot_);

   if (!restored.is_object() || restored.value("id", "").empty()) {
      return false;
   }

   // 初回と再実行で同じ登録IDと保存状態を保持する。
   restoredObjectId_ = restored.value("id", "");
   snapshot_ = std::move(restored);
   return true;
}

bool RestoreObjectSnapshotCommand::Undo(EditorCommandTarget& context) {
   const bool particle = snapshot_.value("objectType", "Model") == "ParticleSystem";
   RemovedEditorEntity removed;

   if (!context.Remove(restoredObjectId_, particle, removed)) {
      return false;
   }

   // Undo前の全状態を保持し、Redoで同じID・設定・親を復元する。
   snapshot_ = std::move(removed.snapshot);
   return true;
}

ModifyComponentCommand::ModifyComponentCommand(std::string objectId, std::string typeName, bool add)
   : objectId_(std::move(objectId)), typeName_(std::move(typeName)), add_(add) {
}

bool ModifyComponentCommand::Execute(EditorCommandTarget& context) {
   if (!context.Contains(objectId_, false) || typeName_.empty()) {
      return false;
   }

   // 依存Componentの追加・削除も含めて戻せるよう、操作前のObject全体を一度だけ保存する。
   if (beforeSnapshot_.is_null()) {
      beforeSnapshot_ = context.Capture(objectId_, false);
   }

   return context.ModifyComponent(objectId_, typeName_, add_);
}

bool ModifyComponentCommand::Undo(EditorCommandTarget& context) {
   return beforeSnapshot_.is_object() && context.ApplySnapshot(objectId_, beforeSnapshot_, false);
}

} // namespace GameEngine

#endif
