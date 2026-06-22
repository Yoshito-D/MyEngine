#include "pch.h"
#include "EditorSceneContext.h"

#ifdef USE_IMGUI

#include "Component/MaterialComponent.h"
#include "Component/ModelAssetComponent.h"
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Effect/ParticleSystem.h"
#include "Framework/EngineContext.h"
#include "Model/Model.h"
#include "Object/Skybox/Skybox.h"
#include "Scene/Camera/Camera.h"
#include "Sprite/Sprite.h"
#include "externals/imgui/ImGuizmo/ImGuizmo.h"
#include "externals/imgui/imgui.h"
#include <fstream>
#include <limits>
#include <string>

namespace GameEngine {

namespace {
ImGuizmo::OPERATION ToImGuizmoOperation(EditorSceneContext::GizmoOperation operation) {
   switch (operation) {
      case EditorSceneContext::GizmoOperation::Rotate:
         return ImGuizmo::ROTATE;
      case EditorSceneContext::GizmoOperation::Scale:
         return ImGuizmo::SCALE;
      case EditorSceneContext::GizmoOperation::Translate:
      default:
         return ImGuizmo::TRANSLATE;
   }
}

ImGuizmo::MODE ToImGuizmoMode(EditorSceneContext::GizmoMode mode) {
   return mode == EditorSceneContext::GizmoMode::World ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
}

Transform MatrixToTransform(const Matrix4x4& matrix) {
   float translation[3]{};
   float rotationDegrees[3]{};
   float scale[3]{};
   ImGuizmo::DecomposeMatrixToComponents(&matrix.m[0][0], translation, rotationDegrees, scale);

   Transform transform{};
   transform.translation = Vector3(translation[0], translation[1], translation[2]);
   transform.scale = Vector3(scale[0], scale[1], scale[2]);
   transform.SetRotationEuler(Vector3(
      ToRadians(rotationDegrees[0]),
      ToRadians(rotationDegrees[1]),
      ToRadians(rotationDegrees[2])));
   return transform;
}

float AbsDiff(float lhs, float rhs) {
   return std::abs(lhs - rhs);
}
} // namespace

void EditorSceneContext::Initialize(std::string sceneName) {
   sceneName_ = sceneName.empty() ? "Scene" : std::move(sceneName);
   hasAutoLoaded_ = false;
   isDirty_ = false;
   SetStatus("Editor scene initialized");
   selectedObject_ = nullptr;
   selectedParticleSystem_ = nullptr;
   commandStack_.Clear();
   hiddenSceneObjects_.clear();
   hiddenParticleSystems_.clear();
   assetRegistry_.Scan();
}

void EditorSceneContext::AutoLoad() {
   if (hasAutoLoaded_) {
      return;
   }

   hasAutoLoaded_ = true;
   Load();
}

void EditorSceneContext::Clear() {
   selectedObject_ = nullptr;
   selectedParticleSystem_ = nullptr;
   manipulatingObject_ = nullptr;
   manipulatingParticleSystem_ = nullptr;
   isManipulating_ = false;
   isManipulatingParticleSystem_ = false;
   commandStack_.Clear();
   objectStore_.Clear();
   hiddenSceneObjects_.clear();
   hiddenParticleSystems_.clear();
   ClearDirty();
   SetStatus("Editor scene cleared");
}

bool EditorSceneContext::Save() {
   const std::filesystem::path filePath = GetSceneFilePath();
   std::error_code error;
   std::filesystem::create_directories(filePath.parent_path(), error);
   if (error) {
      SetStatus("Save failed: could not create directory " + filePath.parent_path().generic_string());
      return false;
   }

   nlohmann::json sceneData = nlohmann::json::object();
   sceneData["version"] = 1;
   sceneData["sceneName"] = sceneName_;
   sceneData["objects"] = objectStore_.SerializeAll();

   std::ofstream file(filePath);
   if (!file.is_open()) {
      SetStatus("Save failed: could not open " + filePath.generic_string());
      return false;
   }

   file << sceneData.dump(3);
   ClearDirty();
   SetStatus("Saved scene: " + filePath.generic_string());
   return true;
}

bool EditorSceneContext::Load() {
   const std::filesystem::path filePath = GetSceneFilePath();
   if (!std::filesystem::exists(filePath)) {
      SetStatus("Load skipped: scene file does not exist " + filePath.generic_string());
      return false;
   }

   std::ifstream file(filePath);
   if (!file.is_open()) {
      SetStatus("Load failed: could not open " + filePath.generic_string());
      return false;
   }

   nlohmann::json sceneData;
   try {
      file >> sceneData;
   } catch (...) {
      SetStatus("Load failed: invalid json " + filePath.generic_string());
      return false;
   }

   if (!sceneData.is_object()) {
      SetStatus("Load failed: root json is not an object");
      return false;
   }

   selectedObject_ = nullptr;
   selectedParticleSystem_ = nullptr;
   commandStack_.Clear();
   objectStore_.Clear();
   hiddenSceneObjects_.clear();
   hiddenParticleSystems_.clear();

   if (sceneData.contains("objects") && sceneData.at("objects").is_array()) {
      for (const auto& objectData : sceneData.at("objects")) {
         objectStore_.RestoreObject(objectData);
      }
   }

   ClearDirty();
   SetStatus("Loaded scene: " + filePath.generic_string());
   return true;
}

std::filesystem::path EditorSceneContext::GetSceneFilePath() const {
   return std::filesystem::path("resources") / "scenes" / (sceneName_ + ".json");
}

void EditorSceneContext::MarkDirty() {
   isDirty_ = true;
}

void EditorSceneContext::ClearDirty() {
   isDirty_ = false;
}

std::vector<Object*> EditorSceneContext::CollectEditableObjects() const {
   std::vector<Object*> objects;

   const auto& models = Model::GetRegisteredModels();
   objects.reserve(models.size() + Sprite::GetRegisteredSprites().size() + Skybox::GetRegisteredSkyboxes().size());

   for (auto* model : models) {
      if (model && !hiddenSceneObjects_.contains(model)) {
         objects.push_back(model);
      }
   }

   for (auto* sprite : Sprite::GetRegisteredSprites()) {
      if (sprite && !hiddenSceneObjects_.contains(sprite)) {
         objects.push_back(sprite);
      }
   }

   for (auto* skybox : Skybox::GetRegisteredSkyboxes()) {
      if (skybox && !hiddenSceneObjects_.contains(skybox)) {
         objects.push_back(skybox);
      }
   }

   return objects;
}

std::vector<ParticleSystem*> EditorSceneContext::CollectEditableParticleSystems() const {
   std::vector<ParticleSystem*> particleSystems;
   const auto& registered = ParticleSystem::GetRegisteredParticleSystems();
   particleSystems.reserve(registered.size());
   for (auto* particleSystem : registered) {
      if (particleSystem && !hiddenParticleSystems_.contains(particleSystem)) {
         particleSystems.push_back(particleSystem);
      }
   }
   return particleSystems;
}

void EditorSceneContext::SelectObject(Object* object) {
   if (object && !IsObjectAlive(object)) {
      selectedObject_ = nullptr;
      return;
   }
   selectedObject_ = object;
   if (selectedObject_) {
      selectedParticleSystem_ = nullptr;
   }
}

void EditorSceneContext::SelectParticleSystem(ParticleSystem* particleSystem) {
   if (particleSystem && !IsParticleSystemAlive(particleSystem)) {
      selectedParticleSystem_ = nullptr;
      return;
   }
   selectedParticleSystem_ = particleSystem;
   if (selectedParticleSystem_) {
      selectedObject_ = nullptr;
   }
}

bool EditorSceneContext::CanDeleteSelectedObject() const {
   return CanDeleteObject(selectedObject_);
}

bool EditorSceneContext::CanDeleteObject(const Object* object) const {
   return object && IsObjectAlive(object);
}

bool EditorSceneContext::CanDeleteParticleSystem(const ParticleSystem* particleSystem) const {
   return particleSystem && IsParticleSystemAlive(particleSystem);
}

void EditorSceneContext::CreateModelFromAsset(const std::string& assetId) {
   commandStack_.Execute(std::make_unique<CreateModelCommand>(assetId, BuildPlacementTransformInFrontOfCamera()), *this);
}

void EditorSceneContext::CreateSpriteFromTexture(const std::string& textureAssetId) {
   commandStack_.Execute(std::make_unique<CreateSpriteCommand>(textureAssetId, BuildPlacementTransformInFrontOfCamera()), *this);
}

ParticleSystem* EditorSceneContext::CreateParticleSystemFromAsset(const std::string& assetId) {
   commandStack_.Execute(std::make_unique<CreateParticleSystemCommand>(assetId, BuildPlacementTransformInFrontOfCamera()), *this);
   return selectedParticleSystem_;
}

void EditorSceneContext::DuplicateSelectedObject() {
   if (selectedParticleSystem_) {
      const std::string particleId = objectStore_.GetId(selectedParticleSystem_);
      if (particleId.empty()) {
         SetStatus("Duplicate failed: selected particle is not editor-owned");
         return;
      }

      nlohmann::json snapshot = objectStore_.SerializeObject(particleId);
      ApplyDuplicateOffset(snapshot);
      snapshot.erase("id");
      commandStack_.Execute(std::make_unique<RestoreObjectSnapshotCommand>(std::move(snapshot), "Duplicate Particle System"), *this);
      return;
   }

   if (!selectedObject_) {
      return;
   }

   const std::string objectId = objectStore_.GetId(selectedObject_);
   if (objectId.empty()) {
      SetStatus("Duplicate failed: selected object is not editor-owned");
      return;
   }

   nlohmann::json snapshot = objectStore_.SerializeObject(objectId);
   ApplyDuplicateOffset(snapshot);
   snapshot.erase("id");
   commandStack_.Execute(std::make_unique<RestoreObjectSnapshotCommand>(std::move(snapshot), "Duplicate Object"), *this);
}

void EditorSceneContext::DeleteObject(Object* object) {
   if (!CanDeleteObject(object)) {
      return;
   }

   if (!objectStore_.Contains(object)) {
      HideSceneOwnedObject(object);
      if (selectedObject_ == object) {
         selectedObject_ = nullptr;
      }
      return;
   }

   const std::string objectId = objectStore_.GetId(object);
   commandStack_.Execute(std::make_unique<DeleteObjectCommand>(objectId), *this);
}

void EditorSceneContext::DeleteParticleSystem(ParticleSystem* particleSystem) {
   if (!CanDeleteParticleSystem(particleSystem)) {
      return;
   }

   if (!objectStore_.Contains(particleSystem)) {
      HideSceneOwnedParticleSystem(particleSystem);
      if (selectedParticleSystem_ == particleSystem) {
         selectedParticleSystem_ = nullptr;
      }
      MarkDirty();
      return;
   }

   const std::string objectId = objectStore_.GetId(particleSystem);
   commandStack_.Execute(std::make_unique<DeleteParticleSystemCommand>(objectId), *this);
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
   if (!selectedObject_) {
      return;
   }

   const std::string objectId = GetObjectIdForCommand(selectedObject_);
   commandStack_.Execute(std::make_unique<AddComponentCommand>(objectId, selectedObject_, typeName), *this);
}

void EditorSceneContext::SetModelAsset(Object* object, const std::string& assetId) {
   if (!object || assetId.empty()) {
      return;
   }

   auto* modelAssetComponent = object->GetComponent<ModelAssetComponent>();
   if (!modelAssetComponent) {
      return;
   }

   const std::string beforeAssetId = modelAssetComponent->GetAssetId();
   if (beforeAssetId == assetId) {
      return;
   }

   commandStack_.Execute(std::make_unique<SetModelAssetCommand>(
      GetObjectIdForCommand(object),
      object,
      beforeAssetId,
      assetId), *this);
}

void EditorSceneContext::SetMaterialTexture(Object* object, size_t slot, const std::string& textureAssetId) {
   if (!object) {
      return;
   }

   auto* materialComponent = object->GetComponent<MaterialComponent>();
   if (!materialComponent) {
      return;
   }

   const std::string beforeTextureId = materialComponent->GetTextureName(slot);
   if (beforeTextureId == textureAssetId) {
      return;
   }

   commandStack_.Execute(std::make_unique<SetMaterialTextureCommand>(
      GetObjectIdForCommand(object),
      object,
      slot,
      beforeTextureId,
      textureAssetId), *this);
}

void EditorSceneContext::Undo() {
   commandStack_.Undo(*this);
}

void EditorSceneContext::Redo() {
   commandStack_.Redo(*this);
}

void EditorSceneContext::DrawTransformGizmo(float viewportX, float viewportY, float viewportWidth, float viewportHeight) {
   if (selectedObject_ && !IsObjectAlive(selectedObject_)) {
      selectedObject_ = nullptr;
   }
   if (selectedParticleSystem_ && !IsParticleSystemAlive(selectedParticleSystem_)) {
      selectedParticleSystem_ = nullptr;
   }

   if ((!selectedObject_ && !selectedParticleSystem_) || viewportWidth <= 0.0f || viewportHeight <= 0.0f) {
      return;
   }

   Camera* camera = EngineContext::GetActiveCamera();
   if (!camera) {
      return;
   }

   if (selectedParticleSystem_) {
      auto* shapeModule = selectedParticleSystem_->GetShapeModule();
      if (!shapeModule) {
         return;
      }

      Matrix4x4 worldMatrix = MakeAffineMatrix(shapeModule->GetTransform());
      Matrix4x4 viewMatrix = camera->GetViewMatrix();
      Matrix4x4 projectionMatrix = camera->GetProjectionMatrix();

      const Transform beforeCall = shapeModule->GetTransform();

      ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
      ImGuizmo::SetRect(viewportX, viewportY, viewportWidth, viewportHeight);
      ImGuizmo::Manipulate(
         &viewMatrix.m[0][0],
         &projectionMatrix.m[0][0],
         ToImGuizmoOperation(gizmoOperation_),
         ToImGuizmoMode(gizmoMode_),
         &worldMatrix.m[0][0]);

      if (ImGuizmo::IsUsing()) {
         if (!isManipulatingParticleSystem_ || manipulatingParticleSystem_ != selectedParticleSystem_) {
            particleTransformBeforeManipulation_ = beforeCall;
            manipulatingParticleSystem_ = selectedParticleSystem_;
            isManipulatingParticleSystem_ = true;
         }

         shapeModule->SetTransform(MatrixToTransform(worldMatrix));
         return;
      }

      if (isManipulatingParticleSystem_) {
         ParticleSystem* manipulatedParticleSystem = manipulatingParticleSystem_;
         manipulatingParticleSystem_ = nullptr;
         isManipulatingParticleSystem_ = false;

         if (manipulatedParticleSystem && manipulatedParticleSystem->GetShapeModule()) {
            SubmitParticleTransformIfNeeded(
               particleTransformBeforeManipulation_,
               manipulatedParticleSystem->GetShapeModule()->GetTransform(),
               manipulatedParticleSystem);
         }
      }
      return;
   }

   auto* transformComponent = selectedObject_->GetComponent<TransformComponent>();
   if (!transformComponent) {
      return;
   }

   Matrix4x4 worldMatrix = MakeAffineMatrix(transformComponent->transform);
   Matrix4x4 viewMatrix = camera->GetViewMatrix();
   Matrix4x4 projectionMatrix = camera->GetProjectionMatrix();

   const Transform beforeCall = transformComponent->transform;

   ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
   ImGuizmo::SetRect(viewportX, viewportY, viewportWidth, viewportHeight);
   ImGuizmo::Manipulate(
      &viewMatrix.m[0][0],
      &projectionMatrix.m[0][0],
      ToImGuizmoOperation(gizmoOperation_),
      ToImGuizmoMode(gizmoMode_),
      &worldMatrix.m[0][0]);

   if (ImGuizmo::IsUsing()) {
      if (!isManipulating_ || manipulatingObject_ != selectedObject_) {
         transformBeforeManipulation_ = beforeCall;
         manipulatingObject_ = selectedObject_;
         isManipulating_ = true;
      }

      transformComponent->transform = MatrixToTransform(worldMatrix);
      return;
   }

   if (isManipulating_) {
      Object* manipulatedObject = manipulatingObject_;
      manipulatingObject_ = nullptr;
      isManipulating_ = false;

      if (manipulatedObject) {
         auto* manipulatedTransform = manipulatedObject->GetComponent<TransformComponent>();
         if (manipulatedTransform) {
            SubmitTransformIfNeeded(transformBeforeManipulation_, manipulatedTransform->transform, manipulatedObject);
         }
      }
   }
}

void EditorSceneContext::AcceptModelAssetDrop() {
   if (!ImGui::BeginDragDropTarget()) {
      return;
   }

   if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_ASSET_MODEL")) {
      const char* assetId = static_cast<const char*>(payload->Data);
      if (assetId && payload->DataSize > 1) {
         CreateModelFromAsset(assetId);
      }
   }
   if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_MODEL_ASSET")) {
      const char* assetId = static_cast<const char*>(payload->Data);
      if (assetId && payload->DataSize > 1) {
         CreateModelFromAsset(assetId);
      }
   }
   if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_ASSET_TEXTURE")) {
      const char* assetId = static_cast<const char*>(payload->Data);
      if (assetId && payload->DataSize > 1) {
         CreateSpriteFromTexture(assetId);
      }
   }
   if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_ASSET_PARTICLE")) {
      const char* assetId = static_cast<const char*>(payload->Data);
      if (assetId && payload->DataSize > 1) {
         CreateParticleSystemFromAsset(assetId);
      }
   }

   ImGui::EndDragDropTarget();
}

void EditorSceneContext::HandleEditorShortcuts() {
   ImGuiIO& io = ImGui::GetIO();
   if (io.WantTextInput) {
      return;
   }

   const bool ctrl = io.KeyCtrl;
   const bool shift = io.KeyShift;
   if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
      Redo();
      return;
   }

   if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
      Redo();
      return;
   }

   if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
      Undo();
      return;
   }

   if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
      DuplicateSelectedObject();
      return;
   }

   if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
      DeleteSelection();
   }
}

void EditorSceneContext::HandleViewportClickSelection(float viewportX, float viewportY, float viewportWidth, float viewportHeight) {
   if (viewportWidth <= 0.0f || viewportHeight <= 0.0f) {
      return;
   }

   if (!ImGui::IsWindowHovered() || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      return;
   }

   if (selectedObject_ && (ImGuizmo::IsOver() || ImGuizmo::IsUsing())) {
      return;
   }

   ImVec2 mouse = ImGui::GetMousePos();
   if (mouse.x < viewportX || mouse.x > viewportX + viewportWidth ||
      mouse.y < viewportY || mouse.y > viewportY + viewportHeight) {
      return;
   }

   Camera* camera = EngineContext::GetActiveCamera();
   if (!camera) {
      return;
   }

   Object* nearestObject = nullptr;
   float nearestDistanceSquared = std::numeric_limits<float>::max();
   constexpr float kMinPickRadiusPixels = 24.0f;
   constexpr float kMaxPickRadiusPixels = 220.0f;
   const Matrix4x4 viewProjection = camera->GetViewProjectionMatrix();

   for (Object* object : CollectEditableObjects()) {
      if (!object) {
         continue;
      }

      const auto* transformComponent = object->GetComponent<TransformComponent>();
      if (!transformComponent) {
         continue;
      }

      const Vector3 screenPosition = Project(
         transformComponent->transform.translation,
         viewportX,
         viewportY,
         viewportWidth,
         viewportHeight,
         viewProjection);

      if (screenPosition.z < 0.0f || screenPosition.z > 1.0f) {
         continue;
      }

      const Vector3 scale = transformComponent->transform.scale;
      const float worldRadius = std::max({ std::abs(scale.x), std::abs(scale.y), std::abs(scale.z), 1.0f }) * 0.5f;
      float pickRadiusPixels = kMinPickRadiusPixels;
      const Vector3 worldCenter = transformComponent->transform.translation;
      const Vector3 sampleOffsets[] = {
         Vector3(worldRadius, 0.0f, 0.0f),
         Vector3(0.0f, worldRadius, 0.0f),
         Vector3(0.0f, 0.0f, worldRadius),
      };

      for (const auto& offset : sampleOffsets) {
         const Vector3 sample = Project(
            worldCenter + offset,
            viewportX,
            viewportY,
            viewportWidth,
            viewportHeight,
            viewProjection);
         if (sample.z < 0.0f || sample.z > 1.0f) {
            continue;
         }

         const float sx = sample.x - screenPosition.x;
         const float sy = sample.y - screenPosition.y;
         pickRadiusPixels = std::max(pickRadiusPixels, std::sqrt(sx * sx + sy * sy));
      }

      pickRadiusPixels = std::clamp(pickRadiusPixels, kMinPickRadiusPixels, kMaxPickRadiusPixels);
      const float pickRadiusSquared = pickRadiusPixels * pickRadiusPixels;
      const float dx = screenPosition.x - mouse.x;
      const float dy = screenPosition.y - mouse.y;
      const float distanceSquared = dx * dx + dy * dy;
      if (distanceSquared <= pickRadiusSquared && distanceSquared < nearestDistanceSquared) {
         nearestDistanceSquared = distanceSquared;
         nearestObject = object;
      }
   }

   SelectObject(nearestObject);
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

void EditorSceneContext::HideSceneOwnedObject(Object* object) {
   if (!object) {
      return;
   }

   hiddenSceneObjects_.insert(object);
   if (auto* renderComponent = object->GetComponent<RenderComponent>()) {
      renderComponent->visible = false;
   }
   MarkDirty();
}

void EditorSceneContext::HideSceneOwnedParticleSystem(ParticleSystem* particleSystem) {
   if (!particleSystem) {
      return;
   }

   hiddenParticleSystems_.insert(particleSystem);
   particleSystem->Stop();
   MarkDirty();
}

bool EditorSceneContext::HasTransformChanged(const Transform& lhs, const Transform& rhs) const {
   constexpr float kEpsilon = 0.0001f;
   const Vector3 lhsRotation = lhs.GetActiveEuler();
   const Vector3 rhsRotation = rhs.GetActiveEuler();
   return
      AbsDiff(lhs.translation.x, rhs.translation.x) > kEpsilon ||
      AbsDiff(lhs.translation.y, rhs.translation.y) > kEpsilon ||
      AbsDiff(lhs.translation.z, rhs.translation.z) > kEpsilon ||
      AbsDiff(lhsRotation.x, rhsRotation.x) > kEpsilon ||
      AbsDiff(lhsRotation.y, rhsRotation.y) > kEpsilon ||
      AbsDiff(lhsRotation.z, rhsRotation.z) > kEpsilon ||
      AbsDiff(lhs.scale.x, rhs.scale.x) > kEpsilon ||
      AbsDiff(lhs.scale.y, rhs.scale.y) > kEpsilon ||
      AbsDiff(lhs.scale.z, rhs.scale.z) > kEpsilon;
}

void EditorSceneContext::SubmitTransformIfNeeded(const Transform& before, const Transform& after, Object* object) {
   if (!object || !HasTransformChanged(before, after)) {
      return;
   }

   commandStack_.Execute(
      std::make_unique<TransformObjectCommand>(GetObjectIdForCommand(object), object, before, after),
      *this);
}

void EditorSceneContext::SubmitParticleTransformIfNeeded(const Transform& before, const Transform& after, ParticleSystem* particleSystem) {
   if (!particleSystem || !HasTransformChanged(before, after)) {
      return;
   }

   commandStack_.Execute(
      std::make_unique<TransformParticleSystemCommand>(GetParticleSystemIdForCommand(particleSystem), particleSystem, before, after),
      *this);
}

Transform EditorSceneContext::BuildPlacementTransformInFrontOfCamera() const {
   Transform transform{};
   Camera* camera = EngineContext::GetActiveCamera();
   if (!camera) {
      return transform;
   }

   constexpr float kPlacementDistance = 8.0f;
   Vector3 forward = camera->GetForward().Normalize();
   if (forward.LengthSquared() < 1e-8f) {
      forward = Vector3(0.0f, 0.0f, 1.0f);
   }

   transform.translation = camera->GetPosition() + forward * kPlacementDistance;
   return transform;
}

std::string EditorSceneContext::GetObjectIdForCommand(const Object* object) const {
   return objectStore_.GetId(object);
}

std::string EditorSceneContext::GetParticleSystemIdForCommand(const ParticleSystem* particleSystem) const {
   return objectStore_.GetId(particleSystem);
}

void EditorSceneContext::SetStatus(std::string message) {
   lastStatusMessage_ = std::move(message);
   if (!lastStatusMessage_.empty()) {
      Logger::GetInstance().Log("[Editor] " + lastStatusMessage_);
   }
}

void EditorSceneContext::ApplyDuplicateOffset(nlohmann::json& snapshot) const {
   if (!snapshot.is_object()) {
      return;
   }

   if (snapshot.contains("components") && snapshot.at("components").is_array()) {
      for (auto& componentData : snapshot.at("components")) {
         if (!componentData.is_object() || componentData.value("typeName", "") != "TransformComponent") {
            continue;
         }
         auto& data = componentData["data"];
         if (!data.is_object() || !data.contains("translation") || !data.at("translation").is_array() || data.at("translation").size() != 3) {
            continue;
         }
         data["translation"][0] = data["translation"][0].get<float>() + 1.0f;
      }
   }

   if (snapshot.value("objectType", "") == "ParticleSystem" &&
      snapshot.contains("data") &&
      snapshot.at("data").contains("shapeModule") &&
      snapshot.at("data").at("shapeModule").contains("position")) {
      auto& position = snapshot["data"]["shapeModule"]["position"];
      if (position.is_array() && position.size() == 3) {
         position[0] = position[0].get<float>() + 1.0f;
      }
   }
}

} // namespace GameEngine

#endif
