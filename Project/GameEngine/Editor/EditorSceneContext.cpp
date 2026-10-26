#include "GameEngine/pch.h"
#include <cstdint>
#include "GameEngine/Editor/EditorSceneContext.h"

#ifdef USE_IMGUI

#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Component/Rendering/RenderComponent.h"
#include "GameEngine/Object/Component/Rendering/LightComponent.h"
#include "GameEngine/Object/Component/UI/UITextComponent.h"
#include "GameEngine/Effects/Particles/ParticleSystem.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Object/Model/Model.h"
#include "GameEngine/Object/Skybox/Skybox.h"
#include "GameEngine/Scene/Camera/Camera.h"
#include "GameEngine/Scene/Camera/Core/CinemachineBrain.h"
#include "GameEngine/Scene/Camera/Core/VirtualCamera.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/Object/Sprite/Sprite.h"
#include "GameEngine/Object/Text/UIText.h"
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Utility/Logger.h"
#include "GameEngine/Utility/JsonFile.h"
#include "ImGuizmo.h"
#include "imgui.h"
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>

#include "GameEngine/Editor/EditorSceneGeometry.h"

namespace GameEngine {

using namespace EditorSceneGeometry;

void EditorSceneContext::Initialize(const SceneDefinition& definition) {
   definition_ = definition;
   isDirty_ = false;
   SetStatus("Editor scene initialized");
   selectedObject_ = nullptr;
   selectedParticleSystem_ = nullptr;
   commandStack_.Clear();

   // 前シーンの非所有ポインターや安定キーを持ち越すと別シーンのEntityへ誤適用されるため一括で破棄する。
   hierarchyOrder_.clear();
   assetRegistry_.Scan();
}

void EditorSceneContext::Suspend() {
   suspendedObjectId_ = selectedObject_ ? selectedObject_->GetEntityId() : std::string{};
   suspendedParticleId_ = Store().GetId(selectedParticleSystem_);
   selectedObject_ = nullptr;
   selectedParticleSystem_ = nullptr;
}

void EditorSceneContext::Rebind(
   SceneWorld& world, const SceneDefinition& definition, const nlohmann::json& data, bool preserve) {
   world_ = &world;
   if (!preserve) {
      Initialize(definition);
   } else {
      definition_ = definition;
   }
   ApplyHierarchyOrder(data.value("hierarchyOrder", nlohmann::json::array()));
   if (preserve) {
      selectedObject_ = world_->FindObjectById(suspendedObjectId_);
      selectedParticleSystem_ = world_->FindParticleSystemById(suspendedParticleId_);
   }
   referencesDirty_ = false;
   ++editRevision_;
}

void EditorSceneContext::Clear() {
   if (auto* audio = EngineContext::GetAudio()) {
      audio->StopPreviews();
   }

   selectedObject_ = nullptr;
   selectedParticleSystem_ = nullptr;
   commandStack_.Clear();
   hierarchyOrder_.clear();
   ClearDirty();
}

bool EditorSceneContext::Save() {
   const std::filesystem::path filePath = GetSceneFilePath();
   try {
      if (!SaveJsonFileAtomically(filePath, SerializeToJson(), kJsonIndentSize)) {
         SetStatus("Save failed: could not write " + filePath.generic_string(), Logger::LogLevel::Error);
         return false;
      }
   } catch (const std::exception& error) {
      SetStatus("Save failed: " + std::string(error.what()), Logger::LogLevel::Error);
      return false;
   }

   // ディスクへの反映が完了してから未保存表示を解除する。
   ClearDirty();
   SetStatus("Saved scene: " + filePath.generic_string());
   return true;
}

nlohmann::json EditorSceneContext::SerializeToJson() {
   auto sceneData = world_->SerializeToJson();
   sceneData["sceneName"] = definition_.name;
   sceneData["hierarchyOrder"] = nlohmann::json::array();

   for (const Object* object : CollectEditableObjects()) {
      sceneData["hierarchyOrder"].push_back(object->GetEntityId());
   }

   return sceneData;
}

void EditorSceneContext::ApplyHierarchyOrder(const nlohmann::json& hierarchyOrderData) {
   hierarchyOrder_.clear();

   if (!hierarchyOrderData.is_array()) return;

   std::unordered_set<std::string> registeredIds;

   for (const Object* object : CollectEditableObjects()) {
      if (object && !object->GetEntityId().empty()) {
         registeredIds.insert(object->GetEntityId());
      }
   }

   // 削除済みIDと重複IDを除外し、壊れた保存データが現在のヒエラルキーへ混入しないようにする。
   for (const auto& objectId : hierarchyOrderData) {
      if (!objectId.is_string()) {
         continue;
      }

      const std::string id = objectId.get<std::string>();

      if (registeredIds.contains(id) &&
          std::find(hierarchyOrder_.begin(), hierarchyOrder_.end(), id) == hierarchyOrder_.end()) {
         hierarchyOrder_.push_back(id);
      }
   }
}

std::filesystem::path EditorSceneContext::GetSceneFilePath() const {
   return definition_.filePath;
}

void EditorSceneContext::MarkDirty() {
   isDirty_ = true;
   referencesDirty_ = true;
}

void EditorSceneContext::RefreshReferences(bool initializeRuntime) {
   if (!referencesDirty_ && !initializeRuntime) return;

   world_->ResolveReferences(CollectEditableObjects(), initializeRuntime);
   referencesDirty_ = false;
}

void EditorSceneContext::ClearDirty() {
   isDirty_ = false;
}

std::vector<Object*> EditorSceneContext::CollectEditableObjects() {
   const auto view = static_cast<const EditorSceneContext&>(*this).CollectEditableObjects();
   std::vector<Object*> editable;
   editable.reserve(view.size());

   for (const auto* object : view) {
      editable.push_back(const_cast<Object*>(object));
   }

   return editable;
}

std::vector<const Object*> EditorSceneContext::CollectEditableObjects() const {
   std::vector<const Object*> objects;
   const auto registeredObjects = Store().GetObjects();
   objects.reserve(registeredObjects.size());

   for (const Object* object : registeredObjects) {
      if (object != nullptr) {
         objects.push_back(object);
      }
   }

   if (!hierarchyOrder_.empty()) {
      std::unordered_map<std::string, size_t> hierarchyRanks;
      hierarchyRanks.reserve(hierarchyOrder_.size());

      for (size_t index = 0; index < hierarchyOrder_.size(); ++index) {
         hierarchyRanks.try_emplace(hierarchyOrder_[index], index);
      }

      // 未登録の新規オブジェクトは生成順のまま末尾へ残し、既存順だけを安定して復元する。
      std::stable_sort(objects.begin(), objects.end(), [&hierarchyRanks](const Object* lhs, const Object* rhs) {
         const auto lhsRank = lhs ? hierarchyRanks.find(lhs->GetEntityId()) : hierarchyRanks.end();
         const auto rhsRank = rhs ? hierarchyRanks.find(rhs->GetEntityId()) : hierarchyRanks.end();
         const bool lhsOrdered = lhsRank != hierarchyRanks.end();
         const bool rhsOrdered = rhsRank != hierarchyRanks.end();

         if (lhsOrdered != rhsOrdered) {
            return lhsOrdered;
         }

         return lhsOrdered && lhsRank->second < rhsRank->second;
      });
   }

   return objects;
}

std::vector<ParticleSystem*> EditorSceneContext::CollectEditableParticleSystems() {
   const auto view = static_cast<const EditorSceneContext&>(*this).CollectEditableParticleSystems();
   std::vector<ParticleSystem*> editable;
   editable.reserve(view.size());

   for (const auto* object : view) {
      editable.push_back(const_cast<ParticleSystem*>(object));
   }

   return editable;
}

std::vector<const ParticleSystem*> EditorSceneContext::CollectEditableParticleSystems() const {
   return Store().GetParticleSystems();
}

void EditorSceneContext::SelectObject(Object* object) {
   if (selectedObject_ != object) {
      if (auto* audio = EngineContext::GetAudio()) {
         audio->StopPreviews();
      }
   }

   if (object && !IsObjectAlive(object)) {
      selectedObject_ = nullptr;
      return;
   }

   selectedObject_ = object;

   // ObjectとParticleSystemは別インスペクター経路なので、選択は常に排他的に保つ。
   if (selectedObject_) {
      selectedParticleSystem_ = nullptr;
   }
}

void EditorSceneContext::SelectParticleSystem(ParticleSystem* particleSystem) {
   if (selectedParticleSystem_ != particleSystem) {
      if (auto* audio = EngineContext::GetAudio()) {
         audio->StopPreviews();
      }
   }

   if (particleSystem && !IsParticleSystemAlive(particleSystem)) {
      selectedParticleSystem_ = nullptr;
      return;
   }

   selectedParticleSystem_ = particleSystem;

   if (selectedParticleSystem_) {
      selectedObject_ = nullptr;
   }
}

bool EditorSceneContext::ReorderObject(Object* movedObject, Object* targetObject, HierarchyDropPosition dropPosition) {
   if (!IsObjectAlive(movedObject) || (targetObject && !IsObjectAlive(targetObject)) || movedObject == targetObject) {
      return false;
   }

   const std::string beforeParentId = movedObject->GetParentEntityId();
   std::vector<std::string> beforeOrder;

   for (const auto* object : CollectEditableObjects()) {
      if (object) {
         beforeOrder.push_back(object->GetEntityId());
      }
   }

   const std::string targetParentId =
      !targetObject ? std::string{}
                    : (dropPosition == HierarchyDropPosition::Into ? targetObject->GetEntityId()
                                                                   : targetObject->GetParentEntityId());
   const std::string movedId = movedObject->GetEntityId();
   std::vector<std::string> orderedIds;

   for (const Object* object : CollectEditableObjects()) {
      if (object && !object->GetEntityId().empty() && object->GetEntityId() != movedId) {
         orderedIds.push_back(object->GetEntityId());
      }
   }

   auto insertionPoint = orderedIds.end();

   if (targetObject) {
      const auto targetIt = std::find(orderedIds.begin(), orderedIds.end(), targetObject->GetEntityId());

      if (targetIt == orderedIds.end()) {
         return false;
      }

      insertionPoint = targetIt;

      if (dropPosition != HierarchyDropPosition::Before) {
         ++insertionPoint;
      }

      if (dropPosition == HierarchyDropPosition::Into) {
         // 登録順が深さ優先とは限らないため、配列全体から最後の直下の子を探す。
         // その後ろへ置けば、ヒエラルキー描画時には常に末尾の子として表示される。
         for (auto candidateIt = targetIt + 1; candidateIt != orderedIds.end(); ++candidateIt) {
            const Object* candidate = Object::FindByEntityId(*candidateIt);

            if (candidate && candidate->GetParentEntityId() == targetObject->GetEntityId()) {
               insertionPoint = candidateIt + 1;
            }
         }
      }
   }

   orderedIds.insert(insertionPoint, movedId);
   return CommitEdit(std::make_unique<ReorderObjectCommand>(
      movedId, beforeParentId, targetParentId, std::move(beforeOrder), std::move(orderedIds)));
}

bool EditorSceneContext::CanDeleteSelectedObject() const {
   return CanDeleteObject(selectedObject_);
}

bool EditorSceneContext::CanDeleteObject(const Object* object) const {
   return object && IsObjectAlive(object);
}

bool EditorSceneContext::CanDeleteParticleSystem(const ParticleSystem* particleSystem) const {
   return particleSystem && IsParticleSystemAlive(particleSystem) && Store().Contains(particleSystem);
}

void EditorSceneContext::CreateEmptyObject() {
   CommitEdit(std::make_unique<CreateObjectCommand>(
      CreateObjectCommand::Kind::Empty, BuildPlacementTransformInFrontOfCamera()));
}

void EditorSceneContext::CreateSpriteFromTexture(const std::string& textureAssetId) {
   const auto* entry = assetRegistry_.FindAsset(textureAssetId, EditorAssetType::Texture);
   if (!entry) return;

   CommitEdit(std::make_unique<CreateObjectCommand>(
      CreateObjectCommand::Kind::Sprite, BuildScreenSpacePlacementTransform(), entry->displayName));
}

void EditorSceneContext::CreateUIText() {
   CommitEdit(
      std::make_unique<CreateObjectCommand>(CreateObjectCommand::Kind::UIText, BuildScreenSpacePlacementTransform()));
}

void EditorSceneContext::CreateSkybox() {
   // Skyboxは不透明な全画面背景で登録順の最後だけが見えるため、既存物があれば新規競合を作らず編集対象にする。
   const auto& registeredSkyboxes = Skybox::GetRegisteredSkyboxes();

   for (auto it = registeredSkyboxes.rbegin(); it != registeredSkyboxes.rend(); ++it) {
      Skybox* skybox = *it;

      if (skybox && IsObjectAlive(skybox)) {
         SelectObject(skybox);
         SetStatus("Selected existing skybox");
         return;
      }
   }

   CommitEdit(std::make_unique<CreateObjectCommand>(CreateObjectCommand::Kind::Skybox));
}

void EditorSceneContext::CreateDirectionalLight() {
   CommitEdit(std::make_unique<CreateObjectCommand>(
      CreateObjectCommand::Kind::DirectionalLight, BuildPlacementTransformInFrontOfCamera()));
}

void EditorSceneContext::CreatePointLight() {
   CommitEdit(std::make_unique<CreateObjectCommand>(
      CreateObjectCommand::Kind::PointLight, BuildPlacementTransformInFrontOfCamera()));
}

void EditorSceneContext::CreateSpotLight() {
   CommitEdit(std::make_unique<CreateObjectCommand>(
      CreateObjectCommand::Kind::SpotLight, BuildPlacementTransformInFrontOfCamera()));
}

void EditorSceneContext::CreateAreaLight() {
   CommitEdit(std::make_unique<CreateObjectCommand>(
      CreateObjectCommand::Kind::AreaLight, BuildPlacementTransformInFrontOfCamera()));
}

bool EditorSceneContext::PlaceAsset(const std::string& assetId, Object* parent) {
   const auto* entry = assetRegistry_.FindAsset(assetId);

   if (!entry || (entry->type != EditorAssetType::Model && entry->type != EditorAssetType::Particle)) {
      SetStatus("Placement failed: only registered models and particles can be placed", Logger::LogLevel::Warning);
      return false;
   }

   if (parent && !IsObjectAlive(parent)) {
      SetStatus("Placement failed: parent no longer exists", Logger::LogLevel::Warning);
      return false;
   }

   if (parent && entry->type == EditorAssetType::Particle) {
      SetStatus("Placement failed: particle systems have no Entity parent; drop on Hierarchy background",
         Logger::LogLevel::Warning);
      return false;
   }

   const auto kind =
      entry->type == EditorAssetType::Model ? CreateObjectCommand::Kind::Model : CreateObjectCommand::Kind::Particle;
   const Transform placement = parent ? Transform{} : BuildPlacementTransformInFrontOfCamera();
   const bool created = CommitEdit(std::make_unique<CreateObjectCommand>(
      kind, placement, entry->assetId, parent ? parent->GetEntityId() : std::string{}));

   if (!created) {
      SetStatus("Placement failed: could not load " + assetId, Logger::LogLevel::Error);
   }

   return created;
}

void EditorSceneContext::DuplicateSelectedObject() {
   if (IsParticleSystemAlive(selectedParticleSystem_)) {
      // 複製もUndo可能にするため、具象型を直接コピーせず復元可能なスナップショットへ統一する。
      auto snapshot = GetParticleSnapshot(selectedParticleSystem_);

      if (!snapshot.is_object() || snapshot.empty()) {
         SetStatus("Duplicate failed: selected particle cannot be duplicated", Logger::LogLevel::Warning);
         return;
      }

      if (snapshot.contains("name") && snapshot.at("name").is_string()) {
         snapshot["name"] = BuildDuplicateName(snapshot.at("name").get<std::string>());
      }

      ApplyDuplicateOffset(snapshot);
      snapshot.erase("id");
      CommitEdit(std::make_unique<RestoreObjectSnapshotCommand>(std::move(snapshot), "Duplicate Particle System"));
      return;
   }

   if (!IsObjectAlive(selectedObject_)) return;

   // 複数Skyboxは描画順だけで勝者が変わるため、保存後に見た目が反転する構成をエディターから作らせない。
   if (dynamic_cast<Skybox*>(selectedObject_)) {
      SetStatus("Duplicate failed: a scene can contain only one skybox", Logger::LogLevel::Warning);
      return;
   }

   auto snapshot = GetObjectSnapshot(selectedObject_);

   if (!snapshot.is_object() || snapshot.empty()) {
      SetStatus("Duplicate failed: selected object cannot be duplicated", Logger::LogLevel::Warning);
      return;
   }

   ApplyDuplicateOffset(snapshot);
   snapshot.erase("id");
   CommitEdit(std::make_unique<RestoreObjectSnapshotCommand>(std::move(snapshot), "Duplicate Object"));
}

void EditorSceneContext::DeleteObject(Object* object) {
   if (!CanDeleteObject(object)) return;

   CommitEdit(std::make_unique<DeleteObjectCommand>(object->GetEntityId()));
}

void EditorSceneContext::DeleteParticleSystem(ParticleSystem* particleSystem) {
   if (!CanDeleteParticleSystem(particleSystem)) return;

   CommitEdit(std::make_unique<DeleteParticleSystemCommand>(GetParticleSystemIdForCommand(particleSystem)));
}

void EditorSceneContext::DeleteSelectedObject() {
   DeleteObject(selectedObject_);
}

void EditorSceneContext::DeleteSelection() {
   if (selectedParticleSystem_) {
      DeleteParticleSystem(selectedParticleSystem_);
      return;
   }

   DeleteSelectedObject();
}

void EditorSceneContext::AddComponentToSelectedObject(const std::string& typeName) {
   if (!IsObjectAlive(selectedObject_)) return;

   const std::string objectId = GetObjectIdForCommand(selectedObject_);
   CommitEdit(std::make_unique<ModifyComponentCommand>(objectId, typeName, true));
}

void EditorSceneContext::RemoveComponentFromSelectedObject(const std::string& typeName) {
   if (!IsObjectAlive(selectedObject_) || typeName.empty()) return;

   const std::string objectId = GetObjectIdForCommand(selectedObject_);
   CommitEdit(std::make_unique<ModifyComponentCommand>(objectId, typeName, false));
}

void EditorSceneContext::Undo() {
   CommandOperations operations(*this);
   FinishCommand(commandStack_.Undo(operations));
}

void EditorSceneContext::Redo() {
   CommandOperations operations(*this);
   FinishCommand(commandStack_.Redo(operations));
}

bool EditorSceneContext::IsObjectAlive(const Object* object) const {
   if (!object) {
      return false;
   }

   const auto objects = CollectEditableObjects();
   return std::find(objects.begin(), objects.end(), object) != objects.end();
}

bool EditorSceneContext::IsParticleSystemAlive(const ParticleSystem* particleSystem) const {
   if (!particleSystem) {
      return false;
   }

   const auto particleSystems = CollectEditableParticleSystems();
   return std::find(particleSystems.begin(), particleSystems.end(), particleSystem) != particleSystems.end();
}

Transform EditorSceneContext::BuildPlacementTransformInFrontOfCamera() const {
   Transform transform{};
   Camera* camera = EngineContext::GetActiveCamera();

   if (!camera) {
      return transform;
   }

   constexpr float kPlacementDistance = 8.0f;
   const Vector3 cameraPosition = ExtractCameraPositionFromView(camera);
   const Vector3 viewForward = ExtractCameraForwardFromView(camera);
   const Vector3 transformForward = NormalizeOrFallback(camera->GetForward(), viewForward);

   // Camera実装間で前方軸の符号規約が異なっても画面内へ置けるよう、両向きを投影して検証する。
   const Vector3 candidateDirections[] = {
      viewForward,
      viewForward * -1.0f,
      transformForward,
      transformForward * -1.0f,
   };

   for (const Vector3& direction : candidateDirections) {
      const Vector3 normalized = NormalizeOrFallback(direction, viewForward);
      const Vector3 candidate = cameraPosition + normalized * kPlacementDistance;

      if (IsProjectedInsideCamera(camera, candidate)) {
         transform.translation = candidate;
         return transform;
      }
   }

   transform.translation = cameraPosition + viewForward * kPlacementDistance;
   return transform;
}

std::string EditorSceneContext::GetObjectIdForCommand(const Object* object) const {
   return object ? object->GetEntityId() : std::string{};
}

std::string EditorSceneContext::GetParticleSystemIdForCommand(const ParticleSystem* particleSystem) {
   return Store().GetId(particleSystem);
}

ParticleSystem* EditorSceneContext::FindParticleForCommand(const std::string& id) const {
   return const_cast<ParticleSystem*>(Store().FindParticleById(id));
}

void EditorSceneContext::SetStatus(std::string message, Logger::LogLevel level) {
   lastStatusMessage_ = std::move(message);

   if (!lastStatusMessage_.empty()) {
      Logger::GetInstance().Log("[Editor] " + lastStatusMessage_, level, Logger::LogChannel::Editor);
   }
}

void EditorSceneContext::ApplyDuplicateOffset(nlohmann::json& snapshot) const {
   if (!snapshot.is_object()) return;

   // Object系とParticleSystem系はTransformの保存場所が異なるため、それぞれのスキーマを個別にずらす。
   if (snapshot.contains("components") && snapshot.at("components").is_array()) {
      for (auto& componentData : snapshot.at("components")) {
         if (!componentData.is_object() || componentData.value("typeName", "") != "TransformComponent") {
            continue;
         }

         auto& data = componentData["data"];

         if (!data.is_object() || !data.contains("translation") || !data.at("translation").is_array() ||
             data.at("translation").size() != 3) {
            continue;
         }

         data["translation"][0] = data["translation"][0].get<float>() + kDuplicatePositionOffset;
      }

      for (auto& componentData : snapshot.at("components")) {
         if (!componentData.is_object() || componentData.value("typeName", "") != "ObjectNameComponent") {
            continue;
         }

         auto& data = componentData["data"];

         if (data.is_object() && data.contains("name") && data.at("name").is_string()) {
            data["name"] = BuildDuplicateName(data.at("name").get<std::string>());
         }
      }
   }

   if (snapshot.value("objectType", "") == "ParticleSystem" && snapshot.contains("data") &&
       snapshot.at("data").contains("shapeModule") && snapshot.at("data").at("shapeModule").contains("position")) {
      auto& position = snapshot["data"]["shapeModule"]["position"];

      if (position.is_array() && position.size() == 3) {
         position[0] = position[0].get<float>() + kDuplicatePositionOffset;
      }
   }
}

nlohmann::json EditorSceneContext::GetObjectSnapshot(const Object* object) {
   return IsObjectAlive(object) ? Store().SerializeObjectState(object) : nlohmann::json::object();
}

nlohmann::json EditorSceneContext::GetParticleSnapshot(const ParticleSystem* particleSystem) {
   return IsParticleSystemAlive(particleSystem) ? Store().SerializeObject(Store().GetId(particleSystem))
                                                : nlohmann::json::object();
}

bool EditorSceneContext::CommitObjectEdit(
   const std::string& id, const nlohmann::json& before, const nlohmann::json& after, std::string name) {
   if (id.empty() || !before.is_object() || !after.is_object() || before == after) {
      return false;
   }

   return CommitEdit(std::make_unique<EditObjectStateCommand>(id, before, after, false, std::move(name)));
}

bool EditorSceneContext::CommitParticleEdit(
   const std::string& id, const nlohmann::json& before, const nlohmann::json& after, std::string name) {
   if (id.empty() || !before.is_object() || !after.is_object() || before == after) {
      return false;
   }

   return CommitEdit(std::make_unique<EditObjectStateCommand>(id, before, after, true, std::move(name)));
}

bool EditorSceneContext::RenameSelectedObject(const std::string& name) {
   if (name.empty()) {
      return false;
   }

   if (selectedObject_ && IsObjectAlive(selectedObject_)) {
      const auto before = GetObjectSnapshot(selectedObject_);
      selectedObject_->SetObjectName(name);
      return CommitObjectEdit(
         selectedObject_->GetEntityId(), before, GetObjectSnapshot(selectedObject_), "Rename Object");
   }

   if (selectedParticleSystem_ && IsParticleSystemAlive(selectedParticleSystem_)) {
      const auto before = GetParticleSnapshot(selectedParticleSystem_);
      selectedParticleSystem_->SetName(name);
      return CommitParticleEdit(
         before.value("id", ""), before, GetParticleSnapshot(selectedParticleSystem_), "Rename Particle System");
   }

   return false;
}

Object* EditorSceneContext::CommandOperations::ResolveObject(const std::string& id) const {
   // 遅延削除されたEntityはグローバル索引に残るため、現シーンの編集対象だけを調べる。
   // 履歴はIDだけを保持し、削除済みEntityや同じ名前の別Entityを誤操作しない。
   for (auto* object : owner_.CollectEditableObjects()) {
      if (object && !id.empty() && object->GetEntityId() == id) {
         return object;
      }
   }

   return nullptr;
}

bool EditorSceneContext::CommandOperations::Contains(const std::string& id, bool particle) const {
   if (!particle) {
      return ResolveObject(id) != nullptr;
   }

   const auto* found = owner_.FindParticleForCommand(id);
   return found && owner_.IsParticleSystemAlive(found);
}

nlohmann::json EditorSceneContext::CommandOperations::Capture(const std::string& id, bool particle) {
   if (particle) {
      auto* found = owner_.FindParticleForCommand(id);
      return found ? owner_.GetParticleSnapshot(found) : nlohmann::json{};
   }

   auto* found = ResolveObject(id);
   return found ? owner_.GetObjectSnapshot(found) : nlohmann::json{};
}

nlohmann::json EditorSceneContext::CommandOperations::Create(CreateObjectCommand::Kind kind, const Transform& pose,
   const std::string& assetId, const std::string& parentId, const nlohmann::json& snapshot) {
   if (!pose.IsFinite()) {
      return {};
   }

   using Kind = CreateObjectCommand::Kind;
   auto& store = owner_.Store();

   if (kind == Kind::Particle) {
      auto* particle =
         snapshot.is_object() ? store.RestoreParticleSystem(snapshot) : store.CreateParticleSystem(assetId, {}, &pose);

      if (!particle) {
         return {};
      }

      owner_.SelectParticleSystem(particle);
      return owner_.GetParticleSnapshot(particle);
   }

   Object* object = nullptr;

   if (snapshot.is_object()) {
      object = store.RestoreObject(snapshot);
   } else {
      switch (kind) {
         case Kind::Model:
            object = store.CreateModel(assetId, &pose);
            break;
         case Kind::Sprite:
            object = store.CreateSprite(assetId, &pose);
            break;
         case Kind::UIText:
            object = store.CreateUIText(&pose);
            break;
         case Kind::Skybox:
            object = store.CreateSkybox();
            break;
         default:
            object = store.CreateGenericObject(&pose);
            break;
      }

      if (object && !parentId.empty()) {
         // 親が削除済みなら別のEntityを親にせず、生成自体を取り消す。
         if (!ResolveObject(parentId) || !object->SetParentEntityId(parentId)) {
            store.DeleteObject(store.GetId(object));
            return {};
         }
      }

      if (object && kind >= Kind::DirectionalLight) {
         auto* light = object->AddComponent<LightComponent>();

         if (!light) {
            store.DeleteObject(store.GetId(object));
            return {};
         }

         const auto type = kind == Kind::DirectionalLight ? LightComponent::Type::Directional
                           : kind == Kind::PointLight     ? LightComponent::Type::Point
                           : kind == Kind::SpotLight      ? LightComponent::Type::Spot
                                                          : LightComponent::Type::Area;
         light->SetLightType(type);
         const char* name = kind == Kind::DirectionalLight ? "Directional Light"
                            : kind == Kind::PointLight     ? "Point Light"
                            : kind == Kind::SpotLight      ? "Spot Light"
                                                           : "Area Light";
         object->SetObjectName(name);
      }
   }

   if (!object) {
      return {};
   }

   owner_.SelectObject(object);
   return owner_.GetObjectSnapshot(object);
}

bool EditorSceneContext::CommandOperations::Remove(const std::string& id, bool particle, RemovedEditorEntity& removed) {
   if (!Contains(id, particle)) {
      return false;
   }

   removed = {};
   removed.snapshot = Capture(id, particle);

   if (particle) {
      auto* found = owner_.FindParticleForCommand(id);
      removed.wasPlaying = found->IsPlaying();
      owner_.SelectParticleSystem(nullptr);
      return owner_.Store().DeleteParticleSystem(id);
   }

   for (auto* child : owner_.CollectEditableObjects()) {
      if (child->GetParentEntityId() == id) {
         removed.childIds.push_back(child->GetEntityId());
      }
   }

   owner_.SelectObject(nullptr);
   return owner_.Store().DeleteObject(id);
}

bool EditorSceneContext::CommandOperations::Restore(const RemovedEditorEntity& removed, bool particle) {
   if (!removed.snapshot.is_object()) {
      return false;
   }

   if (particle) {
      auto* found = owner_.Store().RestoreParticleSystem(removed.snapshot);

      if (!found) {
         return false;
      }

      if (removed.wasPlaying) {
         found->Play();
      } else {
         found->Stop();
      }

      owner_.SelectParticleSystem(found);
      return true;
   }

   auto* object = owner_.Store().RestoreObject(removed.snapshot);

   if (!object) {
      return false;
   }

   for (const auto& childId : removed.childIds) {
      if (auto* child = ResolveObject(childId)) {
         child->SetParentEntityId(object->GetEntityId());
      }
   }

   owner_.SelectObject(object);
   return true;
}

bool EditorSceneContext::CommandOperations::ApplySnapshot(
   const std::string& id, const nlohmann::json& snapshot, bool particle) {
   if (particle) {
      auto* found = owner_.FindParticleForCommand(id);
      return found && owner_.Store().ApplyParticleSystemState(found, snapshot);
   }

   auto* found = ResolveObject(id);
   return found && owner_.Store().ApplyObjectState(found, snapshot);
}

bool EditorSceneContext::CommandOperations::ApplyPose(const std::string& id, const Transform& pose, bool particle) {
   if (!pose.IsFinite()) {
      return false;
   }

   if (particle) {
      auto* found = owner_.FindParticleForCommand(id);

      // ParticleSystemの配置はObject Transformではなく放出ShapeのTransformが正本になっている。
      return found && found->ApplyEmitterPose(pose);
   }

   auto* found = ResolveObject(id);
   auto* transform = found ? found->GetComponent<TransformComponent>() : nullptr;
   return transform && transform->ApplyLocalPose(pose);
}

bool EditorSceneContext::CommandOperations::Reorder(
   const std::string& id, const std::string& parent, const std::vector<std::string>& order) {
   auto* object = ResolveObject(id);

   if (!object || (!parent.empty() && !ResolveObject(parent)) || !object->SetParentEntityId(parent)) {
      return false;
   }

   owner_.ApplyHierarchyOrder(order);
   return true;
}

nlohmann::json EditorSceneContext::CommandOperations::RestoreSnapshot(const nlohmann::json& snapshot) {
   // ParticleSystemはObject階層外で別ストアに登録されるため、種別を見て復元経路を分ける。
   if (snapshot.value("objectType", "Model") == "ParticleSystem") {
      auto* found = owner_.Store().RestoreParticleSystem(snapshot);

      if (!found) {
         return {};
      }

      owner_.SelectObject(nullptr);
      owner_.SelectParticleSystem(found);
      return owner_.GetParticleSnapshot(found);
   }

   auto* found = owner_.Store().RestoreObject(snapshot);

   if (!found) {
      return {};
   }

   owner_.SelectParticleSystem(nullptr);
   owner_.SelectObject(found);
   return owner_.GetObjectSnapshot(found);
}

bool EditorSceneContext::CommandOperations::ModifyComponent(
   const std::string& id, const std::string& typeName, bool add) {
   auto* object = ResolveObject(id);

   if (!object || typeName.empty() || object->HasComponentByTypeName(typeName) == add) {
      return false;
   }

   return add ? object->AddComponentByTypeName(typeName) != nullptr : object->RemoveComponentByTypeName(typeName);
}

void EditorSceneContext::FinishCommand(bool changed) {
   if (!changed) return;

   MarkDirty();
   ++editRevision_;
}

bool EditorSceneContext::CommitEdit(std::unique_ptr<IEditorCommand> command) {
   CommandOperations operations(*this);
   const bool changed = commandStack_.Execute(std::move(command), operations);
   FinishCommand(changed);
   return changed;
}

void EditorSceneContext::FinishEditingFrame() {
   if (!IsObjectAlive(selectedObject_)) {
      selectedObject_ = nullptr;
   }
   if (!IsParticleSystemAlive(selectedParticleSystem_)) {
      selectedParticleSystem_ = nullptr;
   }
}

void EditorSceneContext::RefreshAssets() {
   assetRegistry_.Scan();
}

} // namespace GameEngine

#endif

#ifdef USE_IMGUI
namespace GameEngine {

ParticleSystem* EditorSceneContext::GetSelectedParticleSystem() {
   return IsParticleSystemAlive(selectedParticleSystem_) ? selectedParticleSystem_ : nullptr;
}

const ParticleSystem* EditorSceneContext::GetSelectedParticleSystem() const {
   return IsParticleSystemAlive(selectedParticleSystem_) ? selectedParticleSystem_ : nullptr;
}

} // namespace GameEngine
#endif
