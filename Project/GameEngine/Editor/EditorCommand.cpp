#include "GameEngine/pch.h"
#include "GameEngine/Editor/EditorCommand.h"

#ifdef USE_IMGUI

#include "GameEngine/Object/Component/Rendering/LightComponent.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Editor/EditorSceneContext.h"
#include "GameEngine/Effects/Particles/ParticleSystem.h"
#include "GameEngine/Object/Object.h"

namespace GameEngine {
namespace {
Object* ResolveCommandObject(EditorSceneContext& context, const std::string& id) {
   // 遅延削除されたEntityはグローバル索引に残るため、現シーンの編集対象だけを調べる。
   // 履歴はIDだけを保持し、削除済みEntityや同じ名前の別Entityを誤操作しない。
   for (Object* object : context.CollectEditableObjects()) {
      if (object && !id.empty() && object->GetEntityId() == id) {
         return object;
      }
   }
   return nullptr;
}
} // namespace

bool EditorCommandStack::Execute(std::unique_ptr<IEditorCommand> command, EditorSceneContext& context) {
   if (!command) {
      return false;
   }

   if (!command->Execute(context)) {
      return false;
   }

   // 新しい分岐を実行した時点で以前のRedo履歴は到達不能になるため破棄する。
   undoStack_.push_back(std::move(command));
   redoStack_.clear();
   context.MarkDirty();
   ++context.editRevision_;
   return true;
}

void EditorCommandStack::Undo(EditorSceneContext& context) {
   if (undoStack_.empty()) {
      return;
   }

   // Command自身がUndo中にRedo用スナップショットを更新できるため、同じインスタンスを反対側のStackへ移す。
   auto command = std::move(undoStack_.back());
   undoStack_.pop_back();
   command->Undo(context);
   redoStack_.push_back(std::move(command));
   context.MarkDirty();
   ++context.editRevision_;
}

void EditorCommandStack::Redo(EditorSceneContext& context) {
   if (redoStack_.empty()) {
      return;
   }

   auto command = std::move(redoStack_.back());
   redoStack_.pop_back();
   // 再実行に失敗したCommandは現在状態と整合しないため、Undo履歴へ戻さずこの分岐を打ち切る。
   if (command->Execute(context)) {
      undoStack_.push_back(std::move(command));
      context.MarkDirty();
      ++context.editRevision_;
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

CreateObjectCommand::CreateObjectCommand(Kind kind, Transform transform, std::string assetId, std::string parentId)
   : kind_(kind), initialTransform_(transform), assetId_(std::move(assetId)), parentId_(std::move(parentId)) {
}

const char* CreateObjectCommand::GetName() const {
   switch (kind_) {
      case Kind::Model: return "Create Model";
      case Kind::Sprite: return "Create Sprite";
      case Kind::UIText: return "Create UI Text";
      case Kind::Skybox: return "Create Skybox";
      case Kind::Particle: return "Create Particle System";
      case Kind::DirectionalLight: return "Create Directional Light";
      case Kind::PointLight: return "Create Point Light";
      case Kind::SpotLight: return "Create Spot Light";
      case Kind::AreaLight: return "Create Area Light";
      default: return "Create Empty Object";
   }
}

bool CreateObjectCommand::Execute(EditorSceneContext& context) {
   auto& store = context.CommandObjectStore();
   if (kind_ == Kind::Particle) {
      ParticleSystem* particle = snapshot_.is_object()
         ? store.RestoreParticleSystem(snapshot_)
         : store.CreateParticleSystem(assetId_, {}, &initialTransform_);
      if (!particle) return false;
      objectId_ = store.GetId(particle);
      context.SelectParticleSystem(particle);
      return true;
   }

   Object* object = nullptr;
   if (snapshot_.is_object()) {
      object = store.RestoreObject(snapshot_);
   } else {
      switch (kind_) {
         case Kind::Model: object = store.CreateModel(assetId_, &initialTransform_); break;
         case Kind::Sprite: object = store.CreateSprite(assetId_, &initialTransform_); break;
         case Kind::UIText: object = store.CreateUIText(&initialTransform_); break;
         case Kind::Skybox: object = store.CreateSkybox(); break;
         default: object = store.CreateGenericObject(&initialTransform_); break;
      }
      if (object && !parentId_.empty()) {
         // 親が削除済みなら別のEntityを親にせず、生成自体を取り消す。
         if (!ResolveCommandObject(context, parentId_) || !object->SetParentEntityId(parentId_)) {
            store.DeleteObject(store.GetId(object));
            return false;
         }
      }
      if (object && kind_ >= Kind::DirectionalLight) {
         auto* light = object->AddComponent<LightComponent>();
         if (!light) {
            store.DeleteObject(store.GetId(object));
            return false;
         }
         const auto type = kind_ == Kind::DirectionalLight ? LightComponent::Type::Directional
            : kind_ == Kind::PointLight ? LightComponent::Type::Point
            : kind_ == Kind::SpotLight ? LightComponent::Type::Spot : LightComponent::Type::Area;
         light->SetLightType(type);
         object->SetObjectName(std::string(GetName() + 7));
      }
   }
   if (!object) return false;
   objectId_ = store.GetId(object);
   context.SelectObject(object);
   return true;
}

void CreateObjectCommand::Undo(EditorSceneContext& context) {
   auto& store = context.CommandObjectStore();
   if (!store.ContainsId(objectId_)) return;
   // Undo前の全状態を保持し、Redoも初回と同じID・設定・親を復元する。
   snapshot_ = store.SerializeObject(objectId_);
   if (kind_ == Kind::Particle) {
      if (context.GetSelectedParticleSystem() == store.FindParticleById(objectId_)) context.SelectParticleSystem(nullptr);
      store.DeleteParticleSystem(objectId_);
   } else {
      if (context.GetSelectedObject() == store.FindById(objectId_)) context.SelectObject(nullptr);
      store.DeleteObject(objectId_);
   }
}

EditObjectStateCommand::EditObjectStateCommand(std::string id, nlohmann::json before, nlohmann::json after, bool particle, std::string name)
   : id_(std::move(id)), name_(std::move(name)), before_(std::move(before)), after_(std::move(after)), particle_(particle) {
}

bool EditObjectStateCommand::Execute(EditorSceneContext& context) {
   if (before_ == after_) return false;
   if (alreadyApplied_) {
      if (particle_ ? context.FindParticleForCommand(id_) == nullptr : ResolveCommandObject(context, id_) == nullptr) return false;
      alreadyApplied_ = false;
      return true;
   }
   return Apply(context, after_);
}

void EditObjectStateCommand::Undo(EditorSceneContext& context) { Apply(context, before_); }

bool EditObjectStateCommand::Apply(EditorSceneContext& context, const nlohmann::json& snapshot) {
   if (particle_) {
      auto* particle = context.FindParticleForCommand(id_);
      return particle && context.CommandObjectStore().ApplyParticleSystemState(particle, snapshot);
   }
   auto* object = ResolveCommandObject(context, id_);
   return object && context.CommandObjectStore().ApplyObjectState(object, snapshot);
}

ReorderObjectCommand::ReorderObjectCommand(std::string id, std::string beforeParent, std::string afterParent,
   std::vector<std::string> beforeOrder, std::vector<std::string> afterOrder)
   : id_(std::move(id)), beforeParent_(std::move(beforeParent)), afterParent_(std::move(afterParent)),
     beforeOrder_(std::move(beforeOrder)), afterOrder_(std::move(afterOrder)) {
}

bool ReorderObjectCommand::Execute(EditorSceneContext& context) {
   if (beforeParent_ == afterParent_ && beforeOrder_ == afterOrder_) return false;
   return Apply(context, afterParent_, afterOrder_);
}

void ReorderObjectCommand::Undo(EditorSceneContext& context) { Apply(context, beforeParent_, beforeOrder_); }

bool ReorderObjectCommand::Apply(EditorSceneContext& context, const std::string& parent, const std::vector<std::string>& order) {
   auto* object = ResolveCommandObject(context, id_);
   if (!object || (!parent.empty() && !ResolveCommandObject(context, parent)) || !object->SetParentEntityId(parent)) return false;
   context.ApplyHierarchyOrder(order);
   return true;
}

DeleteObjectCommand::DeleteObjectCommand(std::string objectId)
   : objectId_(std::move(objectId)) {
}

bool DeleteObjectCommand::Execute(EditorSceneContext& context) {
   auto* object = ResolveCommandObject(context, objectId_);
   if (!object) return false;
   sceneOwned_ = !context.CommandObjectStore().Contains(object);
   snapshot_ = context.CommandObjectStore().SerializeObjectState(object, objectId_);
   childIds_.clear();
   for (auto* child : context.CollectEditableObjects()) {
      if (child && child->GetParentEntityId() == objectId_) childIds_.push_back(child->GetEntityId());
   }
   if (context.GetSelectedObject() == object) context.SelectObject(nullptr);
   if (sceneOwned_) {
      sceneKey_ = context.EnsureSceneObjectKey(object);
      context.HideSceneOwnedObject(object);
      for (const auto& id : childIds_) {
         if (auto* child = ResolveCommandObject(context, id)) child->SetParentEntityId({});
      }
      return true;
   }
   return context.CommandObjectStore().DeleteObject(objectId_);
}

void DeleteObjectCommand::Undo(EditorSceneContext& context) {
   if (!snapshot_.is_object()) return;
   Object* object = nullptr;
   if (sceneOwned_) {
      object = context.FindSceneObjectByKey(sceneKey_);
      if (object && context.CommandObjectStore().ApplyObjectState(object, snapshot_)) {
         context.hiddenSceneObjects_.erase(object);
         context.hiddenSceneObjectKeys_.erase(sceneKey_);
      } else return;
   } else {
      object = context.CommandObjectStore().RestoreObject(snapshot_);
   }
   if (object) {
      // 遅延破棄が解除した子の親IDも戻し、削除前と同じ階層を一回のUndoで復元する。
      for (const auto& id : childIds_) {
         if (auto* child = ResolveCommandObject(context, id)) child->SetParentEntityId(object->GetEntityId());
      }
      context.SelectObject(object);
   }
}

DeleteParticleSystemCommand::DeleteParticleSystemCommand(std::string objectId)
   : objectId_(std::move(objectId)) {
}

bool DeleteParticleSystemCommand::Execute(EditorSceneContext& context) {
   auto* particle = context.FindParticleForCommand(objectId_);
   if (!particle || !context.IsParticleSystemAlive(particle)) return false;
   sceneOwned_ = !context.CommandObjectStore().Contains(particle);
   snapshot_ = context.GetParticleSnapshot(particle);
   wasPlaying_ = particle->IsPlaying();
   if (context.GetSelectedParticleSystem() == particle) context.SelectParticleSystem(nullptr);
   if (sceneOwned_) {
      context.HideSceneOwnedParticleSystem(particle);
      return true;
   }
   return context.CommandObjectStore().DeleteParticleSystem(objectId_);
}

void DeleteParticleSystemCommand::Undo(EditorSceneContext& context) {
   if (!snapshot_.is_object()) return;
   ParticleSystem* particle = nullptr;
   if (sceneOwned_) {
      particle = context.FindSceneParticleSystemByKey(objectId_);
      if (!particle || !context.CommandObjectStore().ApplyParticleSystemState(particle, snapshot_)) return;
      context.hiddenParticleSystems_.erase(particle);
      context.hiddenParticleSystemKeys_.erase(objectId_);
   } else {
      particle = context.CommandObjectStore().RestoreParticleSystem(snapshot_);
   }
   if (particle) {
      if (wasPlaying_) particle->Play(); else particle->Stop();
      context.SelectParticleSystem(particle);
   }
}

TransformObjectCommand::TransformObjectCommand(std::string objectId, const Transform& before, const Transform& after)
   : objectId_(std::move(objectId))
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
   return ResolveCommandObject(context, objectId_);
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

   transformComponent->ApplyLocalPose(transform);
}

TransformParticleSystemCommand::TransformParticleSystemCommand(std::string objectId, const Transform& before, const Transform& after)
   : objectId_(std::move(objectId))
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
   return context.FindParticleForCommand(objectId_);
}

void TransformParticleSystemCommand::Apply(EditorSceneContext& context, const Transform& transform) const {
   ParticleSystem* particleSystem = ResolveParticleSystem(context);
   if (!particleSystem || !particleSystem->GetShapeModule()) {
      return;
   }

   // ParticleSystemの配置はObject Transformではなく放出ShapeのTransformが正本になっている。
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
   // ParticleSystemはObject階層外で別ストアに登録されるため、種別を見て復元経路を分ける。
   if (objectType == "ParticleSystem") {
      ParticleSystem* particleSystem = context.CommandObjectStore().RestoreParticleSystem(snapshot_);
      if (!particleSystem) {
         return false;
      }
      restoredObjectId_ = context.CommandObjectStore().GetId(particleSystem);
      snapshot_ = context.CommandObjectStore().SerializeObject(restoredObjectId_);
      context.SelectObject(nullptr);
      context.SelectParticleSystem(particleSystem);
      return true;
   }

   Object* object = context.CommandObjectStore().RestoreObject(snapshot_);
   if (!object) {
      return false;
   }

   restoredObjectId_ = context.CommandObjectStore().GetId(object);
   // 初回に採番された実IDを次回の復元入力にも保持し、複製のRedoで参照先IDを変えない。
   snapshot_ = context.CommandObjectStore().SerializeObject(restoredObjectId_);
   context.SelectParticleSystem(nullptr);
   context.SelectObject(object);
   return true;
}

void RestoreObjectSnapshotCommand::Undo(EditorSceneContext& context) {
   if (restoredObjectId_.empty()) {
      return;
   }

   // Undo直前の状態を保存し、実際の登録IDで取り消す。
   snapshot_ = context.CommandObjectStore().SerializeObject(restoredObjectId_);
   if (ParticleSystem* particleSystem = context.CommandObjectStore().FindParticleById(restoredObjectId_)) {
      if (context.GetSelectedParticleSystem() == particleSystem) {
         context.SelectParticleSystem(nullptr);
      }
      context.CommandObjectStore().DeleteParticleSystem(restoredObjectId_);
      return;
   }

   if (Object* object = context.CommandObjectStore().FindById(restoredObjectId_)) {
      if (context.GetSelectedObject() == object) {
         context.SelectObject(nullptr);
      }
      context.CommandObjectStore().DeleteObject(restoredObjectId_);
   }
}

ModifyComponentCommand::ModifyComponentCommand(std::string objectId, std::string typeName, bool add)
   : objectId_(std::move(objectId)), typeName_(std::move(typeName)), add_(add) {
}

bool ModifyComponentCommand::Execute(EditorSceneContext& context) {
   auto* object = ResolveCommandObject(context, objectId_);
   if (!object || typeName_.empty() || object->HasComponentByTypeName(typeName_) == add_) return false;
   // 依存Componentの追加・削除も含めて戻せるよう、操作前のObject全体を一度だけ保存する。
   if (beforeSnapshot_.is_null()) beforeSnapshot_ = context.GetObjectSnapshot(object);
   return add_ ? object->AddComponentByTypeName(typeName_) != nullptr : object->RemoveComponentByTypeName(typeName_);
}

void ModifyComponentCommand::Undo(EditorSceneContext& context) {
   auto* object = ResolveCommandObject(context, objectId_);
   if (object && beforeSnapshot_.is_object()) context.CommandObjectStore().ApplyObjectState(object, beforeSnapshot_);
}

} // namespace GameEngine

#endif
