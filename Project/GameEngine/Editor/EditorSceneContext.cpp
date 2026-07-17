#include "pch.h"
#include "EditorSceneContext.h"

#ifdef USE_IMGUI

#include "Component/MaterialComponent.h"
#include "Component/Model/ModelAssetComponent.h"
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Component/UI/UITextComponent.h"
#include "Effect/ParticleSystem.h"
#include "Framework/EngineContext.h"
#include "Model/Model.h"
#include "Object/Skybox/Skybox.h"
#include "Scene/Camera/Camera.h"
#include "Scene/Camera/Core/CinemachineBrain.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "Sprite/Sprite.h"
#include "Text/UIText.h"
#include "ImGuizmo.h"
#include "imgui.h"
#include <cmath>
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
   const Vector3 eulerRadians = Vector3(
      ToRadians(rotationDegrees[0]),
      ToRadians(rotationDegrees[1]),
      ToRadians(rotationDegrees[2]));
   transform.SetRotationQuaternion(eulerRadians.ToQuaternion().Normalize());
   return transform;
}

float AbsDiff(float lhs, float rhs) {
   return std::abs(lhs - rhs);
}

std::string GetSceneObjectTypeName(const Object* object) {
   if (dynamic_cast<const UIText*>(object)) {
      return "UIText";
   }
   if (dynamic_cast<const Model*>(object)) {
      return "Model";
   }
   if (dynamic_cast<const Sprite*>(object)) {
      return "Sprite";
   }
   if (dynamic_cast<const Skybox*>(object)) {
      return "Skybox";
   }
   return "Object";
}

std::string BuildSceneKey(const std::string& typeName, const std::string& objectName) {
   return typeName + ":" + (objectName.empty() ? "Object" : objectName);
}

std::string BuildDuplicateName(const std::string& name) {
   const std::string base = name.empty() ? "Object" : name;
   return base + "_Copy";
}

bool IsRegisteredParticleSystem(const ParticleSystem* particleSystem) {
   if (!particleSystem) {
      return false;
   }

   for (auto* registered : ParticleSystem::GetRegisteredParticleSystems()) {
      if (registered == particleSystem) {
         return true;
      }
   }
   return false;
}

bool IsEditableSceneParticleSystem(const ParticleSystem* particleSystem) {
   return particleSystem &&
      particleSystem->IsEditorInspectable() &&
      IsRegisteredParticleSystem(particleSystem);
}

bool StartsWith(const std::string& value, const char* prefix) {
   return value.rfind(prefix, 0) == 0;
}

const nlohmann::json* GetParticleSystemPayload(const nlohmann::json& entry) {
   if (entry.contains("particleSystem") && entry.at("particleSystem").is_object()) {
      return &entry.at("particleSystem");
   }
   return entry.is_object() ? &entry : nullptr;
}

bool IsLegacyEmitterRuntimeParticleEntry(const nlohmann::json& entry) {
   const nlohmann::json* particleData = GetParticleSystemPayload(entry);
   if (!particleData || !particleData->is_object()) {
      return false;
   }

   const std::string objectType = particleData->value("objectType", "");
   const std::string assetId = particleData->value("assetId", "");
   const std::string id = particleData->value("id", "");
   const std::string name = particleData->value("name", "");
   const std::string sceneKey = entry.value("sceneKey", "");

   return objectType == "ParticleSystem" &&
      assetId.empty() &&
      (StartsWith(id, "ParticleSystem:ParticleSystem_") ||
         StartsWith(sceneKey, "ParticleSystem:ParticleSystem_") ||
         StartsWith(name, "ParticleSystem_"));
}

bool IsFiniteVector(const Vector3& value) {
   return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

Vector3 NormalizeOrFallback(const Vector3& value, const Vector3& fallback) {
   if (!IsFiniteVector(value) || value.LengthSquared() < 1e-8f) {
      return fallback;
   }
   return value.Normalize();
}

Vector3 ExtractCameraPositionFromView(const Camera* camera) {
   if (!camera) {
      return {};
   }

   const Matrix4x4 cameraWorld = camera->GetViewMatrix().Inverse();
   const Vector3 position(cameraWorld.m[3][0], cameraWorld.m[3][1], cameraWorld.m[3][2]);
   return IsFiniteVector(position) ? position : camera->GetPosition();
}

Vector3 ExtractCameraForwardFromView(const Camera* camera) {
   if (!camera) {
      return Vector3(0.0f, 0.0f, 1.0f);
   }

   const Matrix4x4 cameraWorld = camera->GetViewMatrix().Inverse();
   const Vector3 forward(cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2]);
   return NormalizeOrFallback(forward, NormalizeOrFallback(camera->GetForward(), Vector3(0.0f, 0.0f, 1.0f)));
}

bool IsProjectedInsideCamera(const Camera* camera, const Vector3& worldPosition) {
   if (!camera || !IsFiniteVector(worldPosition)) {
      return false;
   }

   const Vector4 clip = TransformVectorByMatrix(
      Vector4(worldPosition.x, worldPosition.y, worldPosition.z, 1.0f),
      camera->GetViewProjectionMatrix());
   if (!std::isfinite(clip.x) || !std::isfinite(clip.y) || !std::isfinite(clip.z) || !std::isfinite(clip.w) ||
      std::abs(clip.w) < 1e-6f) {
      return false;
   }

   const float ndcX = clip.x / clip.w;
   const float ndcY = clip.y / clip.w;
   const float ndcZ = clip.z / clip.w;
   return ndcX >= -0.95f && ndcX <= 0.95f &&
      ndcY >= -0.95f && ndcY <= 0.95f &&
      ndcZ >= 0.0f && ndcZ <= 1.0f;
}

bool UsesScreenRenderSpace(const Object* object) {
   if (!dynamic_cast<const Sprite*>(object) && !dynamic_cast<const UIText*>(object)) {
      return false;
   }

   const auto* renderComponent = object->GetComponent<RenderComponent>();
   return renderComponent && renderComponent->renderSpace == RenderComponent::RenderSpace::Screen;
}

Vector2 GetEditorScreenCameraSize(float viewportWidth, float viewportHeight) {
   if (auto* graphicsDevice = EngineContext::GetGraphicsDevice()) {
      const uint32_t width = graphicsDevice->GetBackBufferWidth();
      const uint32_t height = graphicsDevice->GetBackBufferHeight();
      if (width > 0 && height > 0) {
         return Vector2(static_cast<float>(width), static_cast<float>(height));
      }
   }

   return Vector2(std::max(viewportWidth, 1.0f), std::max(viewportHeight, 1.0f));
}

Vector3 CalculateScreenAnchorOffset(Sprite::AnchorPoint anchorPoint, const Vector2& screenSize) {
   const float halfWidth = screenSize.x * 0.5f;
   const float halfHeight = screenSize.y * 0.5f;

   switch (anchorPoint) {
      case Sprite::AnchorPoint::TopLeft:
         return Vector3(-halfWidth, halfHeight, 0.0f);
      case Sprite::AnchorPoint::TopCenter:
         return Vector3(0.0f, halfHeight, 0.0f);
      case Sprite::AnchorPoint::TopRight:
         return Vector3(halfWidth, halfHeight, 0.0f);
      case Sprite::AnchorPoint::MiddleLeft:
         return Vector3(-halfWidth, 0.0f, 0.0f);
      case Sprite::AnchorPoint::MiddleRight:
         return Vector3(halfWidth, 0.0f, 0.0f);
      case Sprite::AnchorPoint::BottomLeft:
         return Vector3(-halfWidth, -halfHeight, 0.0f);
      case Sprite::AnchorPoint::BottomCenter:
         return Vector3(0.0f, -halfHeight, 0.0f);
      case Sprite::AnchorPoint::BottomRight:
         return Vector3(halfWidth, -halfHeight, 0.0f);
      case Sprite::AnchorPoint::MiddleCenter:
      default:
         return Vector3(0.0f, 0.0f, 0.0f);
   }
}

Vector3 CalculateScreenAnchorOffset(UIAnchor anchorPoint, const Vector2& screenSize) {
   const float halfWidth = screenSize.x * 0.5f;
   const float halfHeight = screenSize.y * 0.5f;

   switch (anchorPoint) {
      case UIAnchor::TopLeft:
         return Vector3(-halfWidth, halfHeight, 0.0f);
      case UIAnchor::TopCenter:
         return Vector3(0.0f, halfHeight, 0.0f);
      case UIAnchor::TopRight:
         return Vector3(halfWidth, halfHeight, 0.0f);
      case UIAnchor::MiddleLeft:
         return Vector3(-halfWidth, 0.0f, 0.0f);
      case UIAnchor::MiddleRight:
         return Vector3(halfWidth, 0.0f, 0.0f);
      case UIAnchor::BottomLeft:
         return Vector3(-halfWidth, -halfHeight, 0.0f);
      case UIAnchor::BottomCenter:
         return Vector3(0.0f, -halfHeight, 0.0f);
      case UIAnchor::BottomRight:
         return Vector3(halfWidth, -halfHeight, 0.0f);
      case UIAnchor::MiddleCenter:
      default:
         return Vector3(0.0f, 0.0f, 0.0f);
   }
}

Vector3 GetScreenRenderOffset(const Object* object, const Vector2& screenSize) {
   if (const auto* sprite = dynamic_cast<const Sprite*>(object)) {
      return CalculateScreenAnchorOffset(sprite->GetScreenAnchorPoint(), screenSize);
   }

   if (const auto* uiText = dynamic_cast<const UIText*>(object)) {
      if (const auto* textComponent = uiText->GetComponent<UITextComponent>()) {
         return CalculateScreenAnchorOffset(textComponent->GetStyle().screenAnchor, screenSize);
      }
   }

   return Vector3(0.0f, 0.0f, 0.0f);
}

Vector3 ToEditorScreenWorldPosition(const Vector3& screenTranslation, const Vector3& anchorOffset) {
   // Rendererは下向きYのピクセル座標、Editorの直交投影は上向きYの中央原点を使う。
   return Vector3(
      anchorOffset.x + screenTranslation.x,
      anchorOffset.y - screenTranslation.y,
      anchorOffset.z + screenTranslation.z);
}

Vector3 FromEditorScreenWorldPosition(const Vector3& worldPosition, const Vector3& anchorOffset) {
   return Vector3(
      worldPosition.x - anchorOffset.x,
      anchorOffset.y - worldPosition.y,
      worldPosition.z - anchorOffset.z);
}

Matrix4x4 MakeScreenSpaceProjectionMatrix(const Vector2& screenSize) {
   return MakeOrthographicMatrix(
      -screenSize.x * 0.5f,
      screenSize.y * 0.5f,
      screenSize.x * 0.5f,
      -screenSize.y * 0.5f,
      0.0f,
      100.0f);
}

Transform BuildScreenSpacePlacementTransform() {
   Transform transform{};
   transform.translation.z = 1.0f;
   return transform;
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
   hiddenSceneObjectKeys_.clear();
   hiddenParticleSystemKeys_.clear();
   sceneObjectKeys_.clear();
   sceneParticleSystemKeys_.clear();
   assetRegistry_.Scan();
}

void EditorSceneContext::AutoLoad() {
   if (hasAutoLoaded_) {
      return;
   }

   hasAutoLoaded_ = true;
   RegisterSceneOwnedKeys();
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
   hiddenSceneObjectKeys_.clear();
   hiddenParticleSystemKeys_.clear();
   sceneObjectKeys_.clear();
   sceneParticleSystemKeys_.clear();
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

   nlohmann::json sceneData = SerializeToJson();

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

   if (!LoadFromJson(sceneData)) {
      return false;
   }

   SetStatus("Loaded scene: " + filePath.generic_string());
   return true;
}

nlohmann::json EditorSceneContext::SerializeToJson() {
   nlohmann::json sceneData = nlohmann::json::object();
   sceneData["version"] = 3;
   sceneData["sceneName"] = sceneName_;
   sceneData["objects"] = objectStore_.SerializeAll();
   sceneData["sceneObjects"] = SerializeSceneObjects();
   sceneData["sceneParticleSystems"] = SerializeSceneParticleSystems();
   sceneData["cameras"] = SerializeCameras();
   return sceneData;
}

bool EditorSceneContext::LoadFromJson(const nlohmann::json& sceneData) {
   if (!sceneData.is_object()) {
      SetStatus("Load failed: root json is not an object");
      return false;
   }

   selectedObject_ = nullptr;
   selectedParticleSystem_ = nullptr;
   commandStack_.Clear();
   RegisterSceneOwnedKeys();
   objectStore_.Clear();
   hiddenSceneObjects_.clear();
   hiddenParticleSystems_.clear();
   hiddenSceneObjectKeys_.clear();
   hiddenParticleSystemKeys_.clear();

   if (sceneData.contains("objects") && sceneData.at("objects").is_array()) {
      for (const auto& objectData : sceneData.at("objects")) {
         objectStore_.RestoreObject(objectData);
      }
   }

   if (sceneData.contains("sceneObjects") && sceneData.at("sceneObjects").is_array()) {
      ApplySceneObjects(sceneData.at("sceneObjects"));
   }
   if (sceneData.contains("sceneParticleSystems") && sceneData.at("sceneParticleSystems").is_array()) {
      ApplySceneParticleSystems(sceneData.at("sceneParticleSystems"));
   }
   if (sceneData.contains("cameras") && sceneData.at("cameras").is_object()) {
      ApplyCameras(sceneData.at("cameras"));
   }

   ClearDirty();
   SetStatus("Loaded scene snapshot");
   return true;
}

std::filesystem::path EditorSceneContext::GetSceneFilePath() const {
   return std::filesystem::path("resources") / "game" / "scenes" / (sceneName_ + ".json");
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
   objects.reserve(models.size() + Sprite::GetRegisteredSprites().size() + UIText::GetRegisteredTexts().size() + Skybox::GetRegisteredSkyboxes().size());

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

   for (auto* uiText : UIText::GetRegisteredTexts()) {
      if (uiText && !hiddenSceneObjects_.contains(uiText)) {
         objects.push_back(uiText);
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
      if (IsRegisteredParticleSystem(particleSystem) && !hiddenParticleSystems_.contains(particleSystem)) {
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
   return particleSystem &&
      IsParticleSystemAlive(particleSystem) &&
      (objectStore_.Contains(particleSystem) || IsEditableSceneParticleSystem(particleSystem));
}

void EditorSceneContext::CreateModelFromAsset(const std::string& assetId) {
   commandStack_.Execute(std::make_unique<CreateModelCommand>(assetId, BuildPlacementTransformInFrontOfCamera()), *this);
}

void EditorSceneContext::CreateSpriteFromTexture(const std::string& textureAssetId) {
   commandStack_.Execute(std::make_unique<CreateSpriteCommand>(textureAssetId, BuildScreenSpacePlacementTransform()), *this);
}

void EditorSceneContext::CreateUIText() {
   commandStack_.Execute(std::make_unique<CreateUITextCommand>(BuildScreenSpacePlacementTransform()), *this);
}

ParticleSystem* EditorSceneContext::CreateParticleSystemFromAsset(const std::string& assetId) {
   commandStack_.Execute(std::make_unique<CreateParticleSystemCommand>(assetId, BuildPlacementTransformInFrontOfCamera()), *this);
   return selectedParticleSystem_;
}

void EditorSceneContext::DuplicateSelectedObject() {
   if (selectedParticleSystem_) {
      nlohmann::json snapshot;
      if (const std::string particleId = objectStore_.GetId(selectedParticleSystem_); !particleId.empty()) {
         snapshot = objectStore_.SerializeObject(particleId);
      } else if (IsEditableSceneParticleSystem(selectedParticleSystem_)) {
         snapshot = objectStore_.SerializeParticleSystemState(selectedParticleSystem_);
      }

      if (!snapshot.is_object() || snapshot.empty()) {
         SetStatus("Duplicate failed: selected particle cannot be duplicated");
         return;
      }

      if (snapshot.contains("name") && snapshot.at("name").is_string()) {
         snapshot["name"] = BuildDuplicateName(snapshot.at("name").get<std::string>());
      }
      ApplyDuplicateOffset(snapshot);
      snapshot.erase("id");
      commandStack_.Execute(std::make_unique<RestoreObjectSnapshotCommand>(std::move(snapshot), "Duplicate Particle System"), *this);
      return;
   }

   if (!selectedObject_) {
      return;
   }

   nlohmann::json snapshot;
   if (const std::string objectId = objectStore_.GetId(selectedObject_); !objectId.empty()) {
      snapshot = objectStore_.SerializeObject(objectId);
   } else {
      snapshot = objectStore_.SerializeObjectState(selectedObject_);
   }

   if (!snapshot.is_object() || snapshot.empty()) {
      SetStatus("Duplicate failed: selected object cannot be duplicated");
      return;
   }

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

   const bool useScreenSpace = UsesScreenRenderSpace(selectedObject_);
   const Vector2 screenSize = GetEditorScreenCameraSize(viewportWidth, viewportHeight);
   const Vector3 screenRenderOffset = useScreenSpace ? GetScreenRenderOffset(selectedObject_, screenSize) : Vector3(0.0f, 0.0f, 0.0f);

   Transform gizmoTransform = transformComponent->transform;
   if (useScreenSpace) {
      gizmoTransform.translation = ToEditorScreenWorldPosition(gizmoTransform.translation, screenRenderOffset);
   }

   Matrix4x4 worldMatrix = MakeAffineMatrix(gizmoTransform);
   Matrix4x4 viewMatrix = useScreenSpace ? MakeIdentity4x4() : camera->GetViewMatrix();
   Matrix4x4 projectionMatrix = useScreenSpace ? MakeScreenSpaceProjectionMatrix(screenSize) : camera->GetProjectionMatrix();

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

      Transform manipulatedTransform = MatrixToTransform(worldMatrix);
      if (useScreenSpace) {
         manipulatedTransform.translation = FromEditorScreenWorldPosition(manipulatedTransform.translation, screenRenderOffset);
      }
      transformComponent->transform = manipulatedTransform;
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
   const Vector2 screenSize = GetEditorScreenCameraSize(viewportWidth, viewportHeight);
   const Matrix4x4 screenViewProjection = MakeScreenSpaceProjectionMatrix(screenSize);

   for (Object* object : CollectEditableObjects()) {
      if (!object) {
         continue;
      }

      const auto* transformComponent = object->GetComponent<TransformComponent>();
      if (!transformComponent) {
         continue;
      }

      const bool useScreenSpace = UsesScreenRenderSpace(object);
      const Vector3 screenRenderOffset = useScreenSpace ? GetScreenRenderOffset(object, screenSize) : Vector3(0.0f, 0.0f, 0.0f);
      const Matrix4x4 viewProjection = useScreenSpace ? screenViewProjection : camera->GetViewProjectionMatrix();
      const Vector3 objectCenter = useScreenSpace
         ? ToEditorScreenWorldPosition(transformComponent->transform.translation, screenRenderOffset)
         : transformComponent->transform.translation;

      const Vector3 screenPosition = Project(
         objectCenter,
         viewportX,
         viewportY,
         viewportWidth,
         viewportHeight,
         viewProjection);

      if (screenPosition.z < 0.0f || screenPosition.z > 1.0f) {
         continue;
      }

      const Vector3 scale = transformComponent->transform.scale;
      float worldRadius = std::max({ std::abs(scale.x), std::abs(scale.y), std::abs(scale.z), 1.0f }) * 0.5f;
      if (const auto* sprite = dynamic_cast<const Sprite*>(object); sprite && useScreenSpace) {
         const Vector2 size = sprite->GetSize();
         worldRadius = std::max({
            std::abs(size.x * scale.x),
            std::abs(size.y * scale.y),
            1.0f
         }) * 0.5f;
      } else if (const auto* uiText = dynamic_cast<const UIText*>(object); uiText && useScreenSpace) {
         if (const auto* textComponent = uiText->GetComponent<UITextComponent>()) {
            const Vector2 size = EngineContext::MeasureText(textComponent->GetText(), textComponent->GetStyle());
            worldRadius = std::max({
               std::abs(size.x * scale.x),
               std::abs(size.y * scale.y),
               1.0f
            }) * 0.5f;
         }
      }
      float pickRadiusPixels = kMinPickRadiusPixels;
      const Vector3 sampleOffsets[] = {
         Vector3(worldRadius, 0.0f, 0.0f),
         Vector3(0.0f, worldRadius, 0.0f),
         Vector3(0.0f, 0.0f, worldRadius),
      };

      for (const auto& offset : sampleOffsets) {
         const Vector3 sample = Project(
            objectCenter + offset,
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

void EditorSceneContext::RegisterSceneOwnedKeys() {
   std::unordered_set<std::string> usedObjectKeys;
   for (const auto& [object, key] : sceneObjectKeys_) {
      if (object && !key.empty()) {
         usedObjectKeys.insert(key);
      }
   }

   auto registerObject = [&](Object* object) {
      if (!object || objectStore_.Contains(object) || sceneObjectKeys_.contains(object)) {
         return;
      }

      const std::string baseKey = BuildSceneKey(GetSceneObjectTypeName(object), object->GetObjectName());
      std::string key = baseKey;
      int suffix = 2;
      while (usedObjectKeys.contains(key)) {
         key = baseKey + "#" + std::to_string(suffix++);
      }
      usedObjectKeys.insert(key);
      sceneObjectKeys_[object] = key;
   };

   for (auto* model : Model::GetRegisteredModels()) {
      registerObject(model);
   }
   for (auto* sprite : Sprite::GetRegisteredSprites()) {
      registerObject(sprite);
   }
   for (auto* uiText : UIText::GetRegisteredTexts()) {
      registerObject(uiText);
   }
   for (auto* skybox : Skybox::GetRegisteredSkyboxes()) {
      registerObject(skybox);
   }

   for (auto it = sceneParticleSystemKeys_.begin(); it != sceneParticleSystemKeys_.end();) {
      if (!IsEditableSceneParticleSystem(it->first) || objectStore_.Contains(it->first)) {
         it = sceneParticleSystemKeys_.erase(it);
      } else {
         ++it;
      }
   }

   for (auto* particleSystem : ParticleSystem::GetRegisteredParticleSystems()) {
      if (!IsEditableSceneParticleSystem(particleSystem) ||
         objectStore_.Contains(particleSystem) ||
         sceneParticleSystemKeys_.contains(particleSystem)) {
         continue;
      }

      std::unordered_set<std::string> usedParticleKeys;
      for (const auto& [registeredParticleSystem, key] : sceneParticleSystemKeys_) {
         if (registeredParticleSystem && !key.empty()) {
            usedParticleKeys.insert(key);
         }
      }

      const std::string baseKey = BuildSceneKey("ParticleSystem", particleSystem->GetName());
      std::string key = baseKey;
      int suffix = 2;
      while (usedParticleKeys.contains(key)) {
         key = baseKey + "#" + std::to_string(suffix++);
      }
      usedParticleKeys.insert(key);
      sceneParticleSystemKeys_[particleSystem] = key;
   }
}

std::string EditorSceneContext::EnsureSceneObjectKey(const Object* object) {
   if (!object) {
      return {};
   }

   RegisterSceneOwnedKeys();
   auto it = sceneObjectKeys_.find(object);
   return it == sceneObjectKeys_.end() ? std::string{} : it->second;
}

std::string EditorSceneContext::EnsureSceneParticleSystemKey(const ParticleSystem* particleSystem) {
   if (!particleSystem) {
      return {};
   }

   RegisterSceneOwnedKeys();
   auto it = sceneParticleSystemKeys_.find(particleSystem);
   return it == sceneParticleSystemKeys_.end() ? std::string{} : it->second;
}

Object* EditorSceneContext::FindSceneObjectByKey(const std::string& key) const {
   if (key.empty()) {
      return nullptr;
   }

   auto isRegistered = [](const Object* object) {
      for (auto* model : Model::GetRegisteredModels()) {
         if (model == object) {
            return true;
         }
      }
      for (auto* sprite : Sprite::GetRegisteredSprites()) {
         if (sprite == object) {
            return true;
         }
      }
      for (auto* uiText : UIText::GetRegisteredTexts()) {
         if (uiText == object) {
            return true;
         }
      }
      for (auto* skybox : Skybox::GetRegisteredSkyboxes()) {
         if (skybox == object) {
            return true;
         }
      }
      return false;
   };

   for (const auto& [object, objectKey] : sceneObjectKeys_) {
      if (objectKey == key && object && !objectStore_.Contains(object) && isRegistered(object)) {
         return const_cast<Object*>(object);
      }
   }
   return nullptr;
}

ParticleSystem* EditorSceneContext::FindSceneParticleSystemByKey(const std::string& key) const {
   if (key.empty()) {
      return nullptr;
   }

   for (const auto& [particleSystem, particleKey] : sceneParticleSystemKeys_) {
      if (particleKey == key &&
         IsEditableSceneParticleSystem(particleSystem) &&
         !objectStore_.Contains(particleSystem)) {
         return const_cast<ParticleSystem*>(particleSystem);
      }
   }
   return nullptr;
}

nlohmann::json EditorSceneContext::SerializeSceneObjects() {
   RegisterSceneOwnedKeys();

   nlohmann::json sceneObjects = nlohmann::json::array();
   std::unordered_set<std::string> emittedKeys;

   for (Object* object : CollectEditableObjects()) {
      if (!object || objectStore_.Contains(object)) {
         continue;
      }

      const std::string key = EnsureSceneObjectKey(object);
      if (key.empty()) {
         continue;
      }

      nlohmann::json entry = nlohmann::json::object();
      entry["sceneKey"] = key;
      entry["deleted"] = false;
      entry["object"] = objectStore_.SerializeObjectState(object, key);
      sceneObjects.push_back(std::move(entry));
      emittedKeys.insert(key);
   }

   for (const auto& key : hiddenSceneObjectKeys_) {
      if (key.empty() || emittedKeys.contains(key)) {
         continue;
      }
      sceneObjects.push_back(nlohmann::json{
         { "sceneKey", key },
         { "deleted", true }
      });
   }

   return sceneObjects;
}

nlohmann::json EditorSceneContext::SerializeSceneParticleSystems() {
   RegisterSceneOwnedKeys();

   nlohmann::json sceneParticleSystems = nlohmann::json::array();
   std::unordered_set<std::string> emittedKeys;

   for (ParticleSystem* particleSystem : CollectEditableParticleSystems()) {
      if (!IsEditableSceneParticleSystem(particleSystem) || objectStore_.Contains(particleSystem)) {
         continue;
      }

      const std::string key = EnsureSceneParticleSystemKey(particleSystem);
      if (key.empty()) {
         continue;
      }

      nlohmann::json entry = nlohmann::json::object();
      entry["sceneKey"] = key;
      entry["deleted"] = false;
      entry["particleSystem"] = objectStore_.SerializeParticleSystemState(particleSystem, key);
      sceneParticleSystems.push_back(std::move(entry));
      emittedKeys.insert(key);
   }

   for (const auto& key : hiddenParticleSystemKeys_) {
      if (key.empty() || emittedKeys.contains(key)) {
         continue;
      }
      sceneParticleSystems.push_back(nlohmann::json{
         { "sceneKey", key },
         { "deleted", true }
      });
   }

   return sceneParticleSystems;
}

nlohmann::json EditorSceneContext::SerializeCameras() const {
   nlohmann::json camerasData = nlohmann::json::object();

   CinemachineBrain* brain = EngineContext::GetActiveBrain();
   if (!brain) {
      return camerasData;
   }

   camerasData["brain"] = nlohmann::json{
      { "defaultBlendTime", brain->GetDefaultBlendTime() }
   };

   nlohmann::json virtualCameras = nlohmann::json::array();
   const auto& registeredCameras = brain->GetVirtualCameras();
   for (size_t index = 0; index < registeredCameras.size(); ++index) {
      VirtualCamera* camera = registeredCameras[index];
      if (!camera || camera->GetName() == "DebugCamera") {
         continue;
      }

      nlohmann::json cameraData = camera->Serialize();
      cameraData["index"] = index;
      virtualCameras.push_back(std::move(cameraData));
   }

   camerasData["virtualCameras"] = std::move(virtualCameras);
   return camerasData;
}

void EditorSceneContext::ApplySceneObjects(const nlohmann::json& sceneObjectsData) {
   if (!sceneObjectsData.is_array()) {
      return;
   }

   RegisterSceneOwnedKeys();
   for (const auto& entry : sceneObjectsData) {
      if (!entry.is_object()) {
         continue;
      }

      const std::string key = entry.value("sceneKey", "");
      if (key.empty()) {
         continue;
      }

      Object* object = FindSceneObjectByKey(key);
      if (entry.value("deleted", false)) {
         hiddenSceneObjectKeys_.insert(key);
         if (object) {
            hiddenSceneObjects_.insert(object);
            if (auto* renderComponent = object->GetComponent<RenderComponent>()) {
               renderComponent->visible = false;
            }
         }
         continue;
      }

      if (!object) {
         SetStatus("Load warning: scene object not found for key " + key);
         continue;
      }

      hiddenSceneObjects_.erase(object);
      hiddenSceneObjectKeys_.erase(key);
      if (auto* renderComponent = object->GetComponent<RenderComponent>()) {
         renderComponent->visible = true;
      }

      const nlohmann::json* objectData = nullptr;
      if (entry.contains("object") && entry.at("object").is_object()) {
         objectData = &entry.at("object");
      } else {
         objectData = &entry;
      }

      objectStore_.ApplyObjectState(object, *objectData);
   }
}

void EditorSceneContext::ApplySceneParticleSystems(const nlohmann::json& sceneParticlesData) {
   if (!sceneParticlesData.is_array()) {
      return;
   }

   RegisterSceneOwnedKeys();
   for (const auto& entry : sceneParticlesData) {
      if (!entry.is_object()) {
         continue;
      }

      const std::string key = entry.value("sceneKey", "");
      if (key.empty()) {
         continue;
      }

      ParticleSystem* particleSystem = FindSceneParticleSystemByKey(key);
      if (entry.value("deleted", false)) {
         hiddenParticleSystemKeys_.insert(key);
         if (particleSystem) {
            hiddenParticleSystems_.insert(particleSystem);
            particleSystem->Stop();
         }
         continue;
      }

      if (!particleSystem) {
         if (!IsLegacyEmitterRuntimeParticleEntry(entry)) {
            SetStatus("Load warning: scene particle system not found for key " + key);
         }
         continue;
      }

      hiddenParticleSystems_.erase(particleSystem);
      hiddenParticleSystemKeys_.erase(key);

      const nlohmann::json* particleData = nullptr;
      if (entry.contains("particleSystem") && entry.at("particleSystem").is_object()) {
         particleData = &entry.at("particleSystem");
      } else {
         particleData = &entry;
      }

      objectStore_.ApplyParticleSystemState(particleSystem, *particleData);
   }
}

void EditorSceneContext::ApplyCameras(const nlohmann::json& camerasData) {
   if (!camerasData.is_object()) {
      return;
   }

   CinemachineBrain* brain = EngineContext::GetActiveBrain();
   if (!brain) {
      return;
   }

   if (camerasData.contains("brain") && camerasData.at("brain").is_object()) {
      const auto& brainData = camerasData.at("brain");
      if (brainData.contains("defaultBlendTime") && brainData.at("defaultBlendTime").is_number()) {
         brain->SetDefaultBlendTime(brainData.at("defaultBlendTime").get<float>());
      }
   }

   if (!camerasData.contains("virtualCameras") || !camerasData.at("virtualCameras").is_array()) {
      return;
   }

   const auto& registeredCameras = brain->GetVirtualCameras();
   std::unordered_set<VirtualCamera*> appliedCameras;

   for (const auto& cameraData : camerasData.at("virtualCameras")) {
      if (!cameraData.is_object()) {
         continue;
      }

      const std::string cameraName = cameraData.value("name", "");
      if (cameraName == "DebugCamera") {
         continue;
      }

      VirtualCamera* targetCamera = nullptr;
      if (!cameraName.empty()) {
         for (VirtualCamera* camera : registeredCameras) {
            if (camera && camera->GetName() == cameraName && camera->GetName() != "DebugCamera") {
               targetCamera = camera;
               break;
            }
         }
      }

      if (!targetCamera && cameraData.contains("index") && cameraData.at("index").is_number_unsigned()) {
         const size_t index = cameraData.at("index").get<size_t>();
         if (index < registeredCameras.size()) {
            VirtualCamera* candidate = registeredCameras[index];
            if (candidate && candidate->GetName() != "DebugCamera") {
               targetCamera = candidate;
            }
         }
      }

      if (!targetCamera || appliedCameras.contains(targetCamera)) {
         continue;
      }

      targetCamera->Deserialize(cameraData);
      appliedCameras.insert(targetCamera);
   }
}

void EditorSceneContext::HideSceneOwnedObject(Object* object) {
   if (!object) {
      return;
   }

   const std::string key = EnsureSceneObjectKey(object);
   if (!key.empty()) {
      hiddenSceneObjectKeys_.insert(key);
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

   const std::string key = EnsureSceneParticleSystemKey(particleSystem);
   if (!key.empty()) {
      hiddenParticleSystemKeys_.insert(key);
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
   const Vector3 cameraPosition = ExtractCameraPositionFromView(camera);
   const Vector3 viewForward = ExtractCameraForwardFromView(camera);
   const Vector3 transformForward = NormalizeOrFallback(camera->GetForward(), viewForward);

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
   return objectStore_.GetId(object);
}

std::string EditorSceneContext::GetParticleSystemIdForCommand(const ParticleSystem* particleSystem) const {
   return objectStore_.GetId(particleSystem);
}

void EditorSceneContext::SetStatus(std::string message) {
   lastStatusMessage_ = std::move(message);
   if (!lastStatusMessage_.empty()) {
      Logger::Info("[Editor] " + lastStatusMessage_);
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
