#include "GameEngine/pch.h"
#include "GameEngine/Editor/EditorCommand.h"

#ifdef USE_IMGUI

namespace GameEngine {

bool EditorCommandStack::Execute(std::unique_ptr<IEditorCommand> command, EditorCommandTarget& context) {
   if (!command || !command->Execute(context))
      return false;

   // 新しい分岐を実行した時点で以前のRedo履歴は到達不能になるため破棄する。
   undoStack_.push_back(std::move(command));
   redoStack_.clear();
   return true;
}

bool EditorCommandStack::Undo(EditorCommandTarget& context) {
   if (undoStack_.empty())
      return false;

   // Command自身がUndo中にRedo用スナップショットを更新できるため、同じインスタンスを反対側のStackへ移す。
   auto command = std::move(undoStack_.back());
   undoStack_.pop_back();
   command->Undo(context);
   redoStack_.push_back(std::move(command));
   return true;
}

bool EditorCommandStack::Redo(EditorCommandTarget& context) {
   if (redoStack_.empty())
      return false;
   auto command = std::move(redoStack_.back());
   redoStack_.pop_back();

   // 再実行に失敗したCommandは現在状態と整合しないため、Undo履歴へ戻さずこの分岐を打ち切る。
   if (!command->Execute(context))
      return false;
   undoStack_.push_back(std::move(command));
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
   snapshot_ = context.Create(kind_, initialTransform_, assetId_, parentId_, snapshot_);
   if (!snapshot_.is_object())
      return false;
   objectId_ = snapshot_.value("id", "");
   return !objectId_.empty();
}

void CreateObjectCommand::Undo(EditorCommandTarget& context) {
   const bool particle = kind_ == Kind::Particle;
   if (!context.Contains(objectId_, particle))
      return;

   // Undo前の全状態を保持し、Redoも初回と同じID・設定・親を復元する。
   snapshot_ = context.Capture(objectId_, particle);
   RemovedEditorEntity removed;
   context.Remove(objectId_, particle, removed);
}

EditObjectStateCommand::EditObjectStateCommand(
   std::string id, nlohmann::json before, nlohmann::json after, bool particle, std::string name)
   : id_(std::move(id)), name_(std::move(name)), before_(std::move(before)), after_(std::move(after)),
     particle_(particle) {
}

bool EditObjectStateCommand::Execute(EditorCommandTarget& context) {
   if (before_ == after_)
      return false;
   if (alreadyApplied_) {
      if (!context.Contains(id_, particle_))
         return false;
      alreadyApplied_ = false;
      return true;
   }
   return Apply(context, after_);
}

void EditObjectStateCommand::Undo(EditorCommandTarget& context) {
   Apply(context, before_);
}

bool EditObjectStateCommand::Apply(EditorCommandTarget& context, const nlohmann::json& snapshot) {
   return context.ApplySnapshot(id_, snapshot, particle_);
}

ReorderObjectCommand::ReorderObjectCommand(std::string id, std::string beforeParent, std::string afterParent,
   std::vector<std::string> beforeOrder, std::vector<std::string> afterOrder)
   : id_(std::move(id)), beforeParent_(std::move(beforeParent)), afterParent_(std::move(afterParent)),
     beforeOrder_(std::move(beforeOrder)), afterOrder_(std::move(afterOrder)) {
}

bool ReorderObjectCommand::Execute(EditorCommandTarget& context) {
   if (beforeParent_ == afterParent_ && beforeOrder_ == afterOrder_)
      return false;
   return Apply(context, afterParent_, afterOrder_);
}

void ReorderObjectCommand::Undo(EditorCommandTarget& context) {
   Apply(context, beforeParent_, beforeOrder_);
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

void DeleteObjectCommand::Undo(EditorCommandTarget& context) {
   context.Restore(removed_, false);
}

DeleteParticleSystemCommand::DeleteParticleSystemCommand(std::string objectId) : objectId_(std::move(objectId)) {
}

bool DeleteParticleSystemCommand::Execute(EditorCommandTarget& context) {
   return context.Remove(objectId_, true, removed_);
}

void DeleteParticleSystemCommand::Undo(EditorCommandTarget& context) {
   context.Restore(removed_, true);
}

TransformObjectCommand::TransformObjectCommand(std::string objectId, const Transform& before, const Transform& after)
   : objectId_(std::move(objectId)), before_(before), after_(after) {
}

bool TransformObjectCommand::Execute(EditorCommandTarget& context) {
   return context.ApplyPose(objectId_, after_, false);
}

void TransformObjectCommand::Undo(EditorCommandTarget& context) {
   Apply(context, before_);
}

void TransformObjectCommand::Apply(EditorCommandTarget& context, const Transform& transform) const {
   context.ApplyPose(objectId_, transform, false);
}

TransformParticleSystemCommand::TransformParticleSystemCommand(
   std::string objectId, const Transform& before, const Transform& after)
   : objectId_(std::move(objectId)), before_(before), after_(after) {
}

bool TransformParticleSystemCommand::Execute(EditorCommandTarget& context) {
   return context.ApplyPose(objectId_, after_, true);
}

void TransformParticleSystemCommand::Undo(EditorCommandTarget& context) {
   Apply(context, before_);
}

void TransformParticleSystemCommand::Apply(EditorCommandTarget& context, const Transform& transform) const {
   context.ApplyPose(objectId_, transform, true);
}

RestoreObjectSnapshotCommand::RestoreObjectSnapshotCommand(nlohmann::json snapshot, std::string commandName)
   : snapshot_(std::move(snapshot)), commandName_(std::move(commandName)) {
}

bool RestoreObjectSnapshotCommand::Execute(EditorCommandTarget& context) {
   if (!snapshot_.is_object())
      return false;
   snapshot_ = context.RestoreSnapshot(snapshot_);
   if (!snapshot_.is_object())
      return false;

   // 初回に採番された実IDを次回の復元入力にも保持し、複製のRedoで参照先IDを変えない。
   restoredObjectId_ = snapshot_.value("id", "");
   return !restoredObjectId_.empty();
}

void RestoreObjectSnapshotCommand::Undo(EditorCommandTarget& context) {
   const bool particle = snapshot_.value("objectType", "Model") == "ParticleSystem";
   if (!context.Contains(restoredObjectId_, particle))
      return;

   // Undo直前の状態を保存し、実際の登録IDで取り消す。
   snapshot_ = context.Capture(restoredObjectId_, particle);
   RemovedEditorEntity removed;
   context.Remove(restoredObjectId_, particle, removed);
}

ModifyComponentCommand::ModifyComponentCommand(std::string objectId, std::string typeName, bool add)
   : objectId_(std::move(objectId)), typeName_(std::move(typeName)), add_(add) {
}

bool ModifyComponentCommand::Execute(EditorCommandTarget& context) {
   if (!context.Contains(objectId_, false) || typeName_.empty())
      return false;

   // 依存Componentの追加・削除も含めて戻せるよう、操作前のObject全体を一度だけ保存する。
   if (beforeSnapshot_.is_null())
      beforeSnapshot_ = context.Capture(objectId_, false);
   return context.ModifyComponent(objectId_, typeName_, add_);
}

void ModifyComponentCommand::Undo(EditorCommandTarget& context) {
   if (beforeSnapshot_.is_object())
      context.ApplySnapshot(objectId_, beforeSnapshot_, false);
}

} // namespace GameEngine

#endif
