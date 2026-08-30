#include "pch.h"
#include "EditorSceneContext.h"

#ifdef USE_IMGUI

#include "Component/MaterialComponent.h"
#include "Component/LightComponent.h"
#include "Component/MeshComponent.h"
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
#include "Scene/SceneWorld.h"
#include "Sprite/Sprite.h"
#include "Text/UIText.h"
#include "Utility/ImGuiHelper.h"
#include "ImGuizmo.h"
#include "imgui.h"
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

namespace GameEngine {

namespace {
constexpr int kJsonIndentSize = 3;
constexpr float kVectorLengthEpsilonSquared = 1.0e-8f;
constexpr float kHomogeneousCoordinateEpsilon = 1.0e-6f;
constexpr float kSafeNormalizedDeviceCoordinateLimit = 0.95f;
constexpr float kScreenSpaceFarClip = 100.0f;
constexpr float kDefaultScreenSpaceDepth = 1.0f;
constexpr float kDuplicatePositionOffset = 1.0f;

ImGuizmo::OPERATION ToImGuizmoOperation(EditorSceneContext::GizmoOperation operation, bool restrictTo2D = false) {
   if (restrictTo2D) {
      // スクリーン空間の要素は深度軸を操作すると描画順まで変わるため、平面内の自由度だけを公開する。
      switch (operation) {
         case EditorSceneContext::GizmoOperation::Rotate:
            return ImGuizmo::ROTATE_Z;
         case EditorSceneContext::GizmoOperation::Scale:
            return ImGuizmo::SCALE_X | ImGuizmo::SCALE_Y;
         case EditorSceneContext::GizmoOperation::Translate:
         default:
            return ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y;
      }
   }

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
   // ImGuizmoのEuler角は表示上の中間値に留め、エンジン側では合成時に安定するQuaternionを正規化して保持する。
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

   // 旧シーンに保存された実行時サブエミッターは再生成対象ではないため、自動採番名の組み合わせで識別する。
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
   if (!IsFiniteVector(value) || value.LengthSquared() < kVectorLengthEpsilonSquared) {
      return fallback;
   }
   return value.Normalize();
}

Vector3 ExtractCameraPositionFromView(const Camera* camera) {
   if (!camera) {
      return {};
   }

   // ブレンド中のカメラではTransformより実際のView行列が描画位置を正確に表すため、逆行列から位置を取り出す。
   const Matrix4x4 cameraWorld = camera->GetViewMatrix().Inverse();
   const Vector3 position(cameraWorld.m[3][0], cameraWorld.m[3][1], cameraWorld.m[3][2]);
   return IsFiniteVector(position) ? position : camera->GetPosition();
}

Vector3 ExtractCameraForwardFromView(const Camera* camera) {
   if (!camera) {
      return Vector3(0.0f, 0.0f, 1.0f);
   }

   // 配置方向も現在描画中のViewに合わせ、TransformとViewの更新タイミング差で画面外へ生成されるのを避ける。
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
      std::abs(clip.w) < kHomogeneousCoordinateEpsilon) {
      return false;
   }

   const float ndcX = clip.x / clip.w;
   const float ndcY = clip.y / clip.w;
   const float ndcZ = clip.z / clip.w;
   // クリップ境界ぎりぎりを避け、生成直後のオブジェクトが確実に選択できる余白を残す。
   return ndcX >= -kSafeNormalizedDeviceCoordinateLimit && ndcX <= kSafeNormalizedDeviceCoordinateLimit &&
      ndcY >= -kSafeNormalizedDeviceCoordinateLimit && ndcY <= kSafeNormalizedDeviceCoordinateLimit &&
      ndcZ >= 0.0f && ndcZ <= 1.0f;
}

bool UsesScreenRenderSpace(const Object* object) {
   // UITextはRenderComponentを持たない旧シーンでもTextRendererが常にスクリーン空間で描画する。
   if (dynamic_cast<const UIText*>(object)) {
      return true;
   }

   if (!dynamic_cast<const Sprite*>(object)) {
      return false;
   }

   const auto* renderComponent = object->GetComponent<RenderComponent>();
   return renderComponent && renderComponent->renderSpace == RenderComponent::RenderSpace::Screen;
}

Vector2 GetEditorScreenCameraSize(float viewportWidth, float viewportHeight) {
   float outputWidth = std::max(viewportWidth, 1.0f);
   float outputHeight = std::max(viewportHeight, 1.0f);
   // スクリーン空間オブジェクトはバックバッファへ描画されるため、実出力の縦横比を使う。
   if (auto* graphicsDevice = EngineContext::GetGraphicsDevice()) {
      const uint32_t width = graphicsDevice->GetBackBufferWidth();
      const uint32_t height = graphicsDevice->GetBackBufferHeight();
      if (width > 0 && height > 0) {
         outputWidth = static_cast<float>(width);
         outputHeight = static_cast<float>(height);
      }
   }

   const float referenceWidth = static_cast<float>(Window::kUiReferenceWidth);
   const float referenceHeight = static_cast<float>(Window::kUiReferenceHeight);
   const float outputScale = std::min(outputWidth / referenceWidth, outputHeight / referenceHeight);
   return Vector2(outputWidth / outputScale, outputHeight / outputScale);
}

Vector2 GetEditorScreenLayoutSize() {
   return Vector2(
      static_cast<float>(Window::kUiReferenceWidth),
      static_cast<float>(Window::kUiReferenceHeight));
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

   // UITextはレイアウト系と同じY下向き規約を使うため、Sprite用アンカーとは上下の符号が逆になる。
   switch (anchorPoint) {
      case UIAnchor::TopLeft:
         return Vector3(-halfWidth, -halfHeight, 0.0f);
      case UIAnchor::TopCenter:
         return Vector3(0.0f, -halfHeight, 0.0f);
      case UIAnchor::TopRight:
         return Vector3(halfWidth, -halfHeight, 0.0f);
      case UIAnchor::MiddleLeft:
         return Vector3(-halfWidth, 0.0f, 0.0f);
      case UIAnchor::MiddleRight:
         return Vector3(halfWidth, 0.0f, 0.0f);
      case UIAnchor::BottomLeft:
         return Vector3(-halfWidth, halfHeight, 0.0f);
      case UIAnchor::BottomCenter:
         return Vector3(0.0f, halfHeight, 0.0f);
      case UIAnchor::BottomRight:
         return Vector3(halfWidth, halfHeight, 0.0f);
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
   return Vector3(
      anchorOffset.x + screenTranslation.x,
      anchorOffset.y + screenTranslation.y,
      anchorOffset.z + screenTranslation.z);
}

Vector3 FromEditorScreenWorldPosition(const Vector3& worldPosition, const Vector3& anchorOffset) {
   return Vector3(
      worldPosition.x - anchorOffset.x,
      worldPosition.y - anchorOffset.y,
      worldPosition.z - anchorOffset.z);
}

Matrix4x4 MakeScreenSpaceProjectionMatrix(const Vector2& screenSize, bool useDownwardYAxis = false) {
   const float top = useDownwardYAxis ? -screenSize.y * 0.5f : screenSize.y * 0.5f;
   const float bottom = -top;
   return MakeOrthographicMatrix(
      -screenSize.x * 0.5f,
      top,
      screenSize.x * 0.5f,
      bottom,
      0.0f,
      kScreenSpaceFarClip);
}

Transform BuildScreenSpacePlacementTransform() {
   Transform transform{};
   transform.translation.z = kDefaultScreenSpaceDepth;
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
   // 前シーンの非所有ポインターや安定キーを持ち越すと別シーンのEntityへ誤適用されるため一括で破棄する。
   hiddenSceneObjects_.clear();
   hiddenParticleSystems_.clear();
   hiddenSceneObjectKeys_.clear();
   hiddenParticleSystemKeys_.clear();
   sceneObjectKeys_.clear();
   sceneParticleSystemKeys_.clear();
   hierarchyOrder_.clear();
   assetRegistry_.Scan();
}

void EditorSceneContext::AutoLoad() {
   if (hasAutoLoaded_) {
      return;
   }

   hasAutoLoaded_ = true;
   // JSONを適用する前にBaseSceneが生成した実体へキーを割り当て、保存済み状態との対応を固定する。
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
   // シーン所有EntityはStoreから破棄できないため、コンテキスト終了時に一時的な非表示状態だけを元へ戻す。
   for (const Object* hiddenObject : hiddenSceneObjects_) {
      Object* object = const_cast<Object*>(hiddenObject);
      if (!object) {
         continue;
      }
      for (const auto& component : object->GetComponentContainer().GetAll()) {
         if (component) {
            component->SetEnabled(true);
         }
      }
      if (auto* renderComponent = object->GetComponent<RenderComponent>()) {
         renderComponent->visible = true;
      }
   }
   hiddenSceneObjects_.clear();
   hiddenParticleSystems_.clear();
   hiddenSceneObjectKeys_.clear();
   hiddenParticleSystemKeys_.clear();
   sceneObjectKeys_.clear();
   sceneParticleSystemKeys_.clear();
   hierarchyOrder_.clear();
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

   file << sceneData.dump(kJsonIndentSize);
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
   sceneData["version"] = kCurrentSceneFormatVersion;
   sceneData["sceneName"] = sceneName_;
   // Editor生成物とBaseScene所有物は寿命が異なるため、復元可能な所有物と差分適用対象を別配列へ保存する。
   sceneData["objects"] = objectStore_.SerializeAll();
   sceneData["sceneObjects"] = SerializeSceneObjects();
   sceneData["sceneParticleSystems"] = SerializeSceneParticleSystems();
   sceneData["hierarchyOrder"] = nlohmann::json::array();
   for (const Object* object : CollectEditableObjects()) {
      if (object && !object->GetEntityId().empty()) {
         sceneData["hierarchyOrder"].push_back(object->GetEntityId());
      }
   }
   sceneData["cameras"] = SerializeCameras();
   // シーンライトはLightComponentとしてobjects/sceneObjectsへ保存し、環境設定との二重所有を避ける。
   sceneData["environment"] = nlohmann::json::object();
   sceneData["renderSettings"] = EngineContext::SerializePostProcessSceneState();
   return sceneData;
}

bool EditorSceneContext::LoadFromJson(const nlohmann::json& sceneData) {
   if (!sceneData.is_object()) {
      SetStatus("Load failed: root json is not an object");
      return false;
   }

   // Storeの再構築でポインターが無効になり得るため、選択と履歴を先に切り離してから状態を入れ替える。
   selectedObject_ = nullptr;
   selectedParticleSystem_ = nullptr;
   commandStack_.Clear();
   RegisterSceneOwnedKeys();
   objectStore_.Clear();
   // 同じスナップショット内の安定IDを再利用するため、旧Store所有物をグローバルEntity索引からも先に解放する。
   objectStore_.FlushDeferredDeletes();
   hiddenSceneObjects_.clear();
   hiddenParticleSystems_.clear();
   hiddenSceneObjectKeys_.clear();
   hiddenParticleSystemKeys_.clear();
   hierarchyOrder_.clear();

   if (sceneData.contains("objects") && sceneData.at("objects").is_array()) {
      for (const auto& objectData : sceneData.at("objects")) {
         // DataDrivenSceneではobjectsもSceneWorldが既に所有するため、Reload時は同じSkyboxを更新して二重所有を避ける。
         if (objectData.is_object() && objectData.value("objectType", "") == "Skybox") {
            const std::string objectId = objectData.value("id", "");
            if (auto* sceneWorld = SceneWorld::GetCurrent()) {
               if (auto* existingSkybox = dynamic_cast<Skybox*>(sceneWorld->FindObjectById(objectId))) {
                  objectStore_.ApplyObjectState(existingSkybox, objectData);
                  continue;
               }
            }
         }
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
   if (sceneData.contains("environment") && sceneData.at("environment").is_object()) {
      // 旧形式だけがenvironmentにライトを持つため、現在のLightComponentへ移行しながら読み込む。
      const auto& environment = sceneData.at("environment");
      if (environment.contains("lights") && environment.at("lights").is_array()) {
         for (const auto& lightData : environment.at("lights")) {
            if (!lightData.is_object()) {
               continue;
            }
            const std::string id = lightData.value("id", "");
            if (id.empty()) {
               continue;
            }

            Object* entity = Object::FindByEntityId(id);
            if (!entity) {
               entity = objectStore_.CreateGenericObject(nullptr, id);
               if (entity) {
                  entity->SetObjectName(id);
               }
            }
            if (!entity) {
               continue;
            }

            auto* light = entity->GetComponent<LightComponent>();
            if (!light) {
               light = entity->AddComponent<LightComponent>();
            }
            if (light) {
               light->DeserializeLegacy(lightData);
            }
         }
      }
   }
   if (sceneData.contains("renderSettings") &&
      !EngineContext::ApplyPostProcessSceneState(sceneData.at("renderSettings"))) {
      SetStatus("Load failed: invalid render settings");
      return false;
   }
   ApplyHierarchyOrder(sceneData.value("hierarchyOrder", nlohmann::json::array()));

   ClearDirty();
   SetStatus("Loaded scene snapshot");
   return true;
}

void EditorSceneContext::ApplyHierarchyOrder(const nlohmann::json& hierarchyOrderData) {
   hierarchyOrder_.clear();
   if (!hierarchyOrderData.is_array()) {
      return;
   }

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
   const auto& registeredObjects = Object::GetRegisteredObjects();
   objects.reserve(registeredObjects.size());
   for (Object* object : registeredObjects) {
      if (object && !hiddenSceneObjects_.contains(object)) {
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
      std::stable_sort(objects.begin(), objects.end(),
         [&hierarchyRanks](const Object* lhs, const Object* rhs) {
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
   // ObjectとParticleSystemは別インスペクター経路なので、選択は常に排他的に保つ。
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

bool EditorSceneContext::ReorderObject(
   Object* movedObject,
   Object* targetObject,
   HierarchyDropPosition dropPosition) {
   if (!IsObjectAlive(movedObject) ||
      (targetObject && !IsObjectAlive(targetObject)) ||
      movedObject == targetObject) {
      return false;
   }

   const std::string targetParentId = !targetObject
      ? std::string{}
      : (dropPosition == HierarchyDropPosition::Into
         ? targetObject->GetEntityId()
         : targetObject->GetParentEntityId());
   // 親設定側で循環参照を拒否させ、表示順だけが先に変わる半端な状態を作らない。
   if (!movedObject->SetParentEntityId(targetParentId)) {
      return false;
   }

   const std::string movedId = movedObject->GetEntityId();
   std::vector<std::string> orderedIds;
   for (const Object* object : CollectEditableObjects()) {
      if (object && !object->GetEntityId().empty() && object->GetEntityId() != movedId) {
         orderedIds.push_back(object->GetEntityId());
      }
   }

   auto insertionPoint = orderedIds.end();
   if (targetObject) {
      const auto targetIt = std::find(
         orderedIds.begin(), orderedIds.end(), targetObject->GetEntityId());
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
   hierarchyOrder_ = std::move(orderedIds);
   MarkDirty();
   return true;
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

void EditorSceneContext::CreateEmptyObject() {
   commandStack_.Execute(std::make_unique<CreateGenericObjectCommand>(BuildPlacementTransformInFrontOfCamera()), *this);
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

   commandStack_.Execute(std::make_unique<CreateSkyboxCommand>(), *this);
}

void EditorSceneContext::CreateDirectionalLight() {
   const Transform placement = BuildPlacementTransformInFrontOfCamera();
   if (Object* entity = objectStore_.CreateGenericObject(&placement)) {
      entity->SetObjectName("DirectionalLight_" + entity->GetEntityId());
      entity->AddComponent<LightComponent>()->SetLightType(LightComponent::Type::Directional);
      SelectObject(entity);
      MarkDirty();
   }
}

void EditorSceneContext::CreatePointLight() {
   const Transform placement = BuildPlacementTransformInFrontOfCamera();
   if (Object* entity = objectStore_.CreateGenericObject(&placement)) {
      entity->SetObjectName("PointLight_" + entity->GetEntityId());
      entity->AddComponent<LightComponent>()->SetLightType(LightComponent::Type::Point);
      SelectObject(entity);
      MarkDirty();
   }
}

void EditorSceneContext::CreateSpotLight() {
   const Transform placement = BuildPlacementTransformInFrontOfCamera();
   if (Object* entity = objectStore_.CreateGenericObject(&placement)) {
      entity->SetObjectName("SpotLight_" + entity->GetEntityId());
      entity->AddComponent<LightComponent>()->SetLightType(LightComponent::Type::Spot);
      SelectObject(entity);
      MarkDirty();
   }
}

void EditorSceneContext::CreateAreaLight() {
   const Transform placement = BuildPlacementTransformInFrontOfCamera();
   if (Object* entity = objectStore_.CreateGenericObject(&placement)) {
      entity->SetObjectName("AreaLight_" + entity->GetEntityId());
      entity->AddComponent<LightComponent>()->SetLightType(LightComponent::Type::Area);
      SelectObject(entity);
      MarkDirty();
   }
}

ParticleSystem* EditorSceneContext::CreateParticleSystemFromAsset(const std::string& assetId) {
   commandStack_.Execute(std::make_unique<CreateParticleSystemCommand>(assetId, BuildPlacementTransformInFrontOfCamera()), *this);
   return selectedParticleSystem_;
}

void EditorSceneContext::DuplicateSelectedObject() {
   if (selectedParticleSystem_) {
      // 複製もUndo可能にするため、具象型を直接コピーせず復元可能なスナップショットへ統一する。
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

   // 複数Skyboxは描画順だけで勝者が変わるため、保存後に見た目が反転する構成をエディターから作らせない。
   if (dynamic_cast<Skybox*>(selectedObject_)) {
      SetStatus("Duplicate failed: a scene can contain only one skybox");
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
      // BaseSceneが所有する実体は破棄せず、シーン差分に削除墓標を残して更新と描画だけを停止する。
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

void EditorSceneContext::RemoveComponentFromSelectedObject(const std::string& typeName) {
   if (!selectedObject_ || typeName.empty()) {
      return;
   }

   const std::string objectId = GetObjectIdForCommand(selectedObject_);
   commandStack_.Execute(std::make_unique<RemoveComponentCommand>(objectId, selectedObject_, typeName), *this);
}

void EditorSceneContext::SetModelAsset(Object* object, const std::string& assetId) {
   if (!object || assetId.empty()) {
      return;
   }

   auto* meshComponent = object->GetComponent<MeshComponent>();
   if (!meshComponent) {
      return;
   }

   const std::string beforeAssetId = meshComponent->GetAssetId();
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

void EditorSceneContext::DrawGizmoInspectorControls() {
   ImGui::SeparatorText("Guizmo");

   const char* operationLabels[] = {
      ImGuiHelper::Localize({ "移動", "Move" }),
      ImGuiHelper::Localize({ "回転", "Rotate" }),
      ImGuiHelper::Localize({ "拡縮", "Scale" })
   };
   int operation = static_cast<int>(gizmoOperation_);
   if (ImGui::Combo(
      ImGuiHelper::Localize({ "操作", "Operation" }),
      &operation,
      operationLabels,
      static_cast<int>(std::size(operationLabels)))) {
      gizmoOperation_ = static_cast<GizmoOperation>(operation);
   }

   const char* modeLabels[] = {
      ImGuiHelper::Localize({ "ローカル", "Local" }),
      ImGuiHelper::Localize({ "ワールド", "World" })
   };
   int mode = static_cast<int>(gizmoMode_);
   if (ImGui::Combo(
      ImGuiHelper::Localize({ "空間", "Mode" }),
      &mode,
      modeLabels,
      static_cast<int>(std::size(modeLabels)))) {
      gizmoMode_ = static_cast<GizmoMode>(mode);
   }
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
      ImGuizmo::SetOrthographic(camera->GetProjectionType() == Camera::ProjectionType::Orthographic);
      ImGuizmo::Manipulate(
         &viewMatrix.m[0][0],
         &projectionMatrix.m[0][0],
         ToImGuizmoOperation(gizmoOperation_),
         ToImGuizmoMode(gizmoMode_),
         &worldMatrix.m[0][0]);

      if (ImGuizmo::IsUsing()) {
         if (!isManipulatingParticleSystem_ || manipulatingParticleSystem_ != selectedParticleSystem_) {
            // 連続ドラッグの開始値を一度だけ保存し、毎フレームの微小移動を個別の履歴にしない。
            particleTransformBeforeManipulation_ = beforeCall;
            manipulatingParticleSystem_ = selectedParticleSystem_;
            isManipulatingParticleSystem_ = true;
         }

         shapeModule->SetTransform(MatrixToTransform(worldMatrix));
         return;
      }

      if (isManipulatingParticleSystem_) {
         // マウスを離した時点で一つのCommandへ確定し、Undoでドラッグ開始位置まで戻せるようにする。
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
   // UITextは左上原点のY下向き座標で頂点化されるため、SpriteのY上向き投影と分ける。
   const bool useUITextCoordinates = useScreenSpace && dynamic_cast<UIText*>(selectedObject_) != nullptr;
   const Vector2 screenSize = GetEditorScreenCameraSize(viewportWidth, viewportHeight);
   const Vector3 screenRenderOffset = useScreenSpace
      ? GetScreenRenderOffset(selectedObject_, GetEditorScreenLayoutSize())
      : Vector3(0.0f, 0.0f, 0.0f);

   Transform gizmoTransform = transformComponent->transform;
   if (useScreenSpace) {
      // 保存値はアンカー相対、ギズモは画面中心原点で扱うため、操作中だけ同じ座標系へ変換する。
      gizmoTransform.translation = ToEditorScreenWorldPosition(gizmoTransform.translation, screenRenderOffset);
   }

   Matrix4x4 worldMatrix = MakeAffineMatrix(gizmoTransform);
   Matrix4x4 viewMatrix = useScreenSpace ? MakeIdentity4x4() : camera->GetViewMatrix();
   Matrix4x4 projectionMatrix = useScreenSpace ? MakeScreenSpaceProjectionMatrix(screenSize, useUITextCoordinates) : camera->GetProjectionMatrix();

   const Transform beforeCall = transformComponent->transform;

   ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
   ImGuizmo::SetRect(viewportX, viewportY, viewportWidth, viewportHeight);
   ImGuizmo::SetOrthographic(useScreenSpace || camera->GetProjectionType() == Camera::ProjectionType::Orthographic);
   ImGuizmo::Manipulate(
      &viewMatrix.m[0][0],
      &projectionMatrix.m[0][0],
      ToImGuizmoOperation(gizmoOperation_, useUITextCoordinates),
      ToImGuizmoMode(gizmoMode_),
      &worldMatrix.m[0][0]);

   if (ImGuizmo::IsUsing()) {
      if (!isManipulating_ || manipulatingObject_ != selectedObject_) {
         // 選択がドラッグ中に変わっても、開始値と対象を同じ組として保持する。
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
   // InputText編集中の文字操作をUndoや削除コマンドとして誤解釈しない。
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
   const Vector2 screenLayoutSize = GetEditorScreenLayoutSize();
   const Matrix4x4 screenViewProjection = MakeScreenSpaceProjectionMatrix(screenSize);
   const Matrix4x4 uiTextScreenViewProjection = MakeScreenSpaceProjectionMatrix(screenSize, true);

   // 専用Pickingバッファを持たないため、各中心と投影後の概算半径を使って最も近い候補を選ぶ。
   for (Object* object : CollectEditableObjects()) {
      if (!object) {
         continue;
      }

      const auto* transformComponent = object->GetComponent<TransformComponent>();
      if (!transformComponent) {
         continue;
      }

      const bool useScreenSpace = UsesScreenRenderSpace(object);
      const bool useUITextCoordinates = useScreenSpace && dynamic_cast<const UIText*>(object) != nullptr;
      const Vector3 screenRenderOffset = useScreenSpace
         ? GetScreenRenderOffset(object, screenLayoutSize)
         : Vector3(0.0f, 0.0f, 0.0f);
      const Matrix4x4 viewProjection = useScreenSpace
         ? (useUITextCoordinates ? uiTextScreenViewProjection : screenViewProjection)
         : camera->GetViewProjectionMatrix();
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
      // 三軸を投影してPerspectiveや非一様スケールをピクセル半径へ近似する。
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
   // ポインターや表示名だけでは再起動後に対応できないため、シーン所有物へ保存用の安定キーを割り当てる。
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

      const std::string& entityId = object->GetEntityId();
      const std::string baseKey = entityId.rfind("runtime_entity_", 0) == 0
         ? BuildSceneKey(GetSceneObjectTypeName(object), object->GetObjectName())
         : entityId;
      std::string key = baseKey;
      int suffix = 2;
      while (usedObjectKeys.contains(key)) {
         key = baseKey + "#" + std::to_string(suffix++);
      }
      usedObjectKeys.insert(key);
      if (object->GetEntityId().rfind("runtime_entity_", 0) == 0) {
         // 一時IDは起動ごとに変わるので、初回登録時にシーン内で再現可能なキーへ置換する。
         object->SetEntityId(key);
      }
      sceneObjectKeys_[object] = key;
   };

   for (Object* object : Object::GetRegisteredObjects()) {
      registerObject(object);
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
      const auto& registeredObjects = Object::GetRegisteredObjects();
      return std::find(registeredObjects.begin(), registeredObjects.end(), object) != registeredObjects.end();
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

   // 実体が列挙から消えても削除意図を次回ロードへ伝えるため、キーだけの墓標を出力する。
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

      // BaseSceneが生成した実体へ差分を適用し、ここでは所有権を持つ新規Entityを作らない。
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

      // 並び替えに強い名前一致を優先し、旧データや同名不在時だけ保存時のindexへフォールバックする。
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
   // RenderComponentだけでなく更新系Componentも止め、削除済みEntityが副作用を発生させないようにする。
   for (const auto& component : object->GetComponentContainer().GetAll()) {
      if (component) {
         component->SetEnabled(false);
      }
   }
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
   // 行列の分解誤差で空のギズモ操作が履歴化されないよう、編集精度より小さい差を無視する。
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

   // Object系とParticleSystem系はTransformの保存場所が異なるため、それぞれのスキーマを個別にずらす。
   if (snapshot.contains("components") && snapshot.at("components").is_array()) {
      for (auto& componentData : snapshot.at("components")) {
         if (!componentData.is_object() || componentData.value("typeName", "") != "TransformComponent") {
            continue;
         }
         auto& data = componentData["data"];
         if (!data.is_object() || !data.contains("translation") || !data.at("translation").is_array() || data.at("translation").size() != 3) {
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

   if (snapshot.value("objectType", "") == "ParticleSystem" &&
      snapshot.contains("data") &&
      snapshot.at("data").contains("shapeModule") &&
      snapshot.at("data").at("shapeModule").contains("position")) {
      auto& position = snapshot["data"]["shapeModule"]["position"];
      if (position.is_array() && position.size() == 3) {
         position[0] = position[0].get<float>() + kDuplicatePositionOffset;
      }
   }
}

} // namespace GameEngine

#endif
