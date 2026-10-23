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

#include "GameEngine/Editor/EditorSceneSession.h"
#include "GameEngine/Object/Component/Animation/AnimationComponent.h"
#include "GameEngine/Object/Component/Effects/ParticleEmitterComponent.h"
#include "GameEngine/Editor/EditorSceneGeometry.h"

namespace GameEngine {
using namespace EditorSceneGeometry;

namespace {
constexpr std::int32_t kDebugCameraStateFormatVersion = 1;
constexpr std::int32_t kDebugCameraJsonIndentSize = 3;
}

EditorSceneSession::~EditorSceneSession() {
   Detach();
}

void EditorSceneSession::Attach(Scene& scene, const nlohmann::json& data, bool preserve) {
   scene_ = &scene;
   definition_ = scene.GetDefinition();

   if (restoreEditingState_) {
      if (editingContext_) {
         context_ = std::move(editingContext_);
         preserve = true;
      }
      restoreEditingState_ = false;
   }

   if (!context_) {
      context_ = std::make_unique<EditorSceneContext>(scene.GetSceneWorld());
   }
   context_->Rebind(scene.GetSceneWorld(), definition_, data, preserve);
   auto* brain = EngineContext::GetActiveBrain();
   debugCamera_ = std::make_unique<DebugCamera>();
   debugCamera_->Initialize();
   debugCamera_->SetPriority(-1);
   LoadDebugCameraState();
   debugCamera_->SetWorld(&scene.GetSceneWorld());
   brain->RegisterVirtualCamera(debugCamera_.get());
   cameraEditor_ = std::make_unique<CameraEditor>();
   cameraEditor_->Initialize(EngineContext::GetLineRenderer());
   cameraEditor_->SetTargetBrain(brain);
   cameraEditor_->SetWorld(&scene.GetSceneWorld());
}

void EditorSceneSession::Detach() {
   if (!scene_) return;
   context_->Suspend();
   SaveDebugCameraState();

   if (auto* brain = EngineContext::GetActiveBrain()) {
      brain->UnregisterVirtualCamera(debugCamera_.get());
   }
   cameraEditor_.reset();
   debugCamera_.reset();
   scene_ = nullptr;
   isManipulating_ = isManipulatingParticleSystem_ = isDebugCameraActive_ = false;
   manipulatingObject_ = nullptr;
   manipulatingParticleSystem_ = nullptr;
}

void EditorSceneSession::BeginPlay() {
   if (!scene_ || editingContext_) return;
   const auto data = context_->SerializeToJson();
   // 再生中の操作で選択や履歴が書き換わらないよう、編集用Contextを別に保持する。
   context_->Suspend();
   editingContext_ = std::move(context_);
   context_ = std::make_unique<EditorSceneContext>(scene_->GetSceneWorld());
   context_->Rebind(scene_->GetSceneWorld(), definition_, data, false);
}

void EditorSceneSession::RestoreEditingStateOnAttach() {
   restoreEditingState_ = editingContext_ != nullptr;
}

void EditorSceneSession::Update() {
   if (!scene_) return;

   if (!EngineContext::ShouldRunRuntimeUpdate()) {
      UpdateEditorLightProxies(EngineContext::GetUnscaledDeltaTime());
      DrawEditorComponentDebug();
   }

   if (EngineContext::IsKeyTriggered(KeyCode::F1)) {
      isDebugCameraActive_ = !isDebugCameraActive_;
      debugCamera_->SetPriority(isDebugCameraActive_ ? 100 : -1);
   }

   if (auto* brain = EngineContext::GetActiveBrain(); brain && !EngineContext::ShouldRunRuntimeUpdate()) {
      brain->UpdateEditorPreview(
         EngineContext::GetUnscaledDeltaTime(), isDebugCameraActive_ ? debugCamera_.get() : nullptr);
   }

   if (manipulatingObject_ && !context_->IsObjectAlive(manipulatingObject_)) {
      manipulatingObject_ = nullptr;
      isManipulating_ = false;
   }

   if (manipulatingParticleSystem_ && !context_->IsParticleSystemAlive(manipulatingParticleSystem_)) {
      manipulatingParticleSystem_ = nullptr;
      isManipulatingParticleSystem_ = false;
   }
}

void EditorSceneSession::Draw() {
   if (!cameraEditor_) return;
   cameraEditor_->ShowEditorWindow();

   if (auto* brain = EngineContext::GetActiveBrain()) {
      cameraEditor_->DrawGizmos(brain->GetOutputCamera());
   }
}

void EditorSceneSession::UpdateEditorLightProxies(float deltaTime) {
   // Edit停止中は通常のComponent更新が走らないが、ライトのGPUプロキシだけは
   // Inspector編集を即座にViewportへ反映する必要があるため個別に同期する。
   const auto registeredObjects = scene_->GetSceneWorld().GetObjectStore().GetObjects();

   for (GameEngine::Object* object : registeredObjects) {
      if (!object) {
         continue;
      }

      if (auto* light = object->GetComponent<GameEngine::LightComponent>(); light && light->IsEnabled()) {
         light->Update(deltaTime);
      }
   }
}

void EditorSceneSession::DrawEditorComponentDebug() {
   const auto registeredObjects = scene_->GetSceneWorld().GetObjectStore().GetObjects();

   for (GameEngine::Object* object : registeredObjects) {
      if (!object) {
         continue;
      }

      if (auto* animation = object->GetComponent<GameEngine::AnimationComponent>();
         animation && animation->IsEnabled()) {
         animation->DrawDebugBones();
      }

      if (auto* particleEmitter = object->GetComponent<GameEngine::ParticleEmitterComponent>();
         particleEmitter && particleEmitter->IsEnabled()) {
         particleEmitter->DrawDebugAttachments();
      }
   }
}

std::filesystem::path EditorSceneSession::GetDebugCameraStateFilePath() const {
   const std::string filename = definition_.name + ".json";
   return std::filesystem::path("resources") / "game" / "editor" / "debug_cameras" /
          std::filesystem::path(std::u8string(filename.begin(), filename.end()));
}

void EditorSceneSession::LoadDebugCameraState() {
   if (!debugCamera_) return;

   const std::filesystem::path filePath = GetDebugCameraStateFilePath();

   if (!std::filesystem::exists(filePath)) return;

   std::ifstream file(filePath);

   if (!file.is_open()) return;

   nlohmann::json root;
   try {
      file >> root;
   } catch (...) {
      return;
   }

   try {
      if (!root.is_object() || root.value("version", 0) != kDebugCameraStateFormatVersion ||
          !root.contains("debugCamera") || !root.at("debugCamera").is_object()) {
         return;
      }

      // 保存されたpriority/activeを復元するとF1の選択状態まで前回値に引きずられる。
      // Transform等だけを読み、選択制御用の実行時プロパティは初期化時の値を保つ。
      const std::int32_t priority = debugCamera_->GetPriority();
      const bool active = debugCamera_->IsActive();
      debugCamera_->Deserialize(root.at("debugCamera"));
      debugCamera_->SetName("DebugCamera");
      debugCamera_->SetPriority(priority);
      debugCamera_->SetActive(active);
   } catch (const nlohmann::json::exception& error) {
      Logger::Warning(
         "Editor camera state could not be restored: " + std::string(error.what()), Logger::LogChannel::Editor);
   }
}

void EditorSceneSession::SaveDebugCameraState() const {
   if (!debugCamera_) return;

   const std::filesystem::path filePath = GetDebugCameraStateFilePath();
   std::error_code error;
   std::filesystem::create_directories(filePath.parent_path(), error);

   if (error) return;

   std::ofstream file(filePath);

   if (!file.is_open()) return;

   // シーンごとに別ファイルへ保存し、作業中の視点を他シーンへ持ち込まない。
   nlohmann::json root = nlohmann::json::object();
   root["version"] = kDebugCameraStateFormatVersion;
   root["sceneName"] = definition_.name;
   root["debugCamera"] = debugCamera_->Serialize();
   file << root.dump(kDebugCameraJsonIndentSize);
}

void EditorSceneSession::DrawGizmoInspectorControls() {
   ImGui::SeparatorText("Guizmo");

   const char* operationLabels[] = {ImGuiHelper::Localize({"移動", "Move"}), ImGuiHelper::Localize({"回転", "Rotate"}),
      ImGuiHelper::Localize({"拡縮", "Scale"})};
   std::int32_t operation = static_cast<std::int32_t>(gizmoOperation_);

   if (ImGui::Combo(ImGuiHelper::Localize({"操作", "Operation"}), &operation, operationLabels,
          static_cast<std::int32_t>(std::size(operationLabels)))) {
      gizmoOperation_ = static_cast<GizmoOperation>(operation);
   }

   const char* modeLabels[] = {
      ImGuiHelper::Localize({"ローカル", "Local"}), ImGuiHelper::Localize({"ワールド", "World"})};
   std::int32_t mode = static_cast<std::int32_t>(gizmoMode_);

   if (ImGui::Combo(ImGuiHelper::Localize({"空間", "Mode"}), &mode, modeLabels,
          static_cast<std::int32_t>(std::size(modeLabels)))) {
      gizmoMode_ = static_cast<GizmoMode>(mode);
   }
}

void EditorSceneSession::DrawTransformGizmo(
   float viewportX, float viewportY, float viewportWidth, float viewportHeight) {
   if (context_->selectedObject_ && !context_->IsObjectAlive(context_->selectedObject_)) {
      if (auto* audio = EngineContext::GetAudio()) {
         audio->StopPreviews();
      }

      context_->selectedObject_ = nullptr;
   }

   if (context_->selectedParticleSystem_ && !context_->IsParticleSystemAlive(context_->selectedParticleSystem_)) {
      context_->selectedParticleSystem_ = nullptr;
   }

   if ((!context_->selectedObject_ && !context_->selectedParticleSystem_) || viewportWidth <= 0.0f ||
       viewportHeight <= 0.0f) {
      return;
   }

   Camera* camera = EngineContext::GetActiveCamera();

   if (!camera) return;

   if (context_->selectedParticleSystem_) {
      const auto shapeSettings = context_->selectedParticleSystem_->DescribeShapeModule();
      const auto* shapeModule = &shapeSettings;

      Matrix4x4 worldMatrix = MakeAffineMatrix(shapeModule->GetTransform());
      Matrix4x4 viewMatrix = camera->GetViewMatrix();
      Matrix4x4 projectionMatrix = camera->GetProjectionMatrix();

      const Transform beforeCall = shapeModule->GetTransform();

      ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
      ImGuizmo::SetRect(viewportX, viewportY, viewportWidth, viewportHeight);
      ImGuizmo::SetOrthographic(camera->GetProjectionType() == Camera::ProjectionType::Orthographic);
      ImGuizmo::Manipulate(&viewMatrix.m[0][0], &projectionMatrix.m[0][0], ToImGuizmoOperation(gizmoOperation_),
         ToImGuizmoMode(gizmoMode_), &worldMatrix.m[0][0]);

      if (ImGuizmo::IsUsing()) {
         if (!isManipulatingParticleSystem_ || manipulatingParticleSystem_ != context_->selectedParticleSystem_) {
            // 連続ドラッグの開始値を一度だけ保存し、毎フレームの微小移動を個別の履歴にしない。
            particleTransformBeforeManipulation_ = beforeCall;
            manipulatingParticleSystem_ = context_->selectedParticleSystem_;
            isManipulatingParticleSystem_ = true;
         }

         context_->selectedParticleSystem_->ApplyEmitterPose(MatrixToTransform(worldMatrix));
         return;
      }

      if (isManipulatingParticleSystem_) {
         // マウスを離した時点で一つのCommandへ確定し、Undoでドラッグ開始位置まで戻せるようにする。
         ParticleSystem* manipulatedParticleSystem = manipulatingParticleSystem_;
         manipulatingParticleSystem_ = nullptr;
         isManipulatingParticleSystem_ = false;

         if (manipulatedParticleSystem) {
            SubmitParticleTransformIfNeeded(particleTransformBeforeManipulation_,
               manipulatedParticleSystem->DescribeShapeModule().GetTransform(), manipulatedParticleSystem);
         }
      }

      return;
   }

   auto* transformComponent = context_->selectedObject_->GetComponent<TransformComponent>();

   if (!transformComponent) return;

   const bool useScreenSpace = UsesScreenRenderSpace(context_->selectedObject_);

   // UITextは左上原点のY下向き座標で頂点化されるため、SpriteのY上向き投影と分ける。
   const bool useUITextCoordinates = useScreenSpace && dynamic_cast<UIText*>(context_->selectedObject_) != nullptr;
   const Vector2 screenSize = GetEditorScreenCameraSize(viewportWidth, viewportHeight);
   const Vector3 screenRenderOffset = useScreenSpace
                                         ? GetScreenRenderOffset(context_->selectedObject_, GetEditorScreenLayoutSize())
                                         : Vector3(0.0f, 0.0f, 0.0f);

   Transform gizmoTransform = transformComponent->GetLocalPose();

   if (useScreenSpace) {
      // 保存値はアンカー相対、ギズモは画面中心原点で扱うため、操作中だけ同じ座標系へ変換する。
      gizmoTransform.translation = ToEditorScreenWorldPosition(gizmoTransform.translation, screenRenderOffset);
   }

   Matrix4x4 worldMatrix = MakeAffineMatrix(gizmoTransform);
   Matrix4x4 viewMatrix = useScreenSpace ? MakeIdentity4x4() : camera->GetViewMatrix();
   Matrix4x4 projectionMatrix = useScreenSpace ? MakeScreenSpaceProjectionMatrix(screenSize, useUITextCoordinates)
                                               : camera->GetProjectionMatrix();

   const Transform beforeCall = transformComponent->GetLocalPose();

   ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
   ImGuizmo::SetRect(viewportX, viewportY, viewportWidth, viewportHeight);
   ImGuizmo::SetOrthographic(useScreenSpace || camera->GetProjectionType() == Camera::ProjectionType::Orthographic);
   ImGuizmo::Manipulate(&viewMatrix.m[0][0], &projectionMatrix.m[0][0],
      ToImGuizmoOperation(gizmoOperation_, useUITextCoordinates), ToImGuizmoMode(gizmoMode_), &worldMatrix.m[0][0]);

   if (ImGuizmo::IsUsing()) {
      if (!isManipulating_ || manipulatingObject_ != context_->selectedObject_) {
         // 選択がドラッグ中に変わっても、開始値と対象を同じ組として保持する。
         transformBeforeManipulation_ = beforeCall;
         manipulatingObject_ = context_->selectedObject_;
         isManipulating_ = true;
      }

      Transform manipulatedTransform = MatrixToTransform(worldMatrix);

      if (useScreenSpace) {
         manipulatedTransform.translation =
            FromEditorScreenWorldPosition(manipulatedTransform.translation, screenRenderOffset);
      }

      transformComponent->ApplyLocalPose(manipulatedTransform);
      return;
   }

   if (isManipulating_) {
      Object* manipulatedObject = manipulatingObject_;
      manipulatingObject_ = nullptr;
      isManipulating_ = false;

      if (manipulatedObject) {
         auto* manipulatedTransform = manipulatedObject->GetComponent<TransformComponent>();

         if (manipulatedTransform) {
            SubmitTransformIfNeeded(
               transformBeforeManipulation_, manipulatedTransform->GetLocalPose(), manipulatedObject);
         }
      }
   }
}

void EditorSceneSession::AcceptViewportAssetDrop() {
   if (!ImGui::BeginDragDropTarget()) return;

   if (const auto* entry = EditorUI::AcceptAssetDrop(EditorAssetType::Unknown, [](const EditorAssetEntry& entry) {
          return entry.type == EditorAssetType::Model || entry.type == EditorAssetType::Particle;
       })) {
      context_->PlaceAsset(entry->assetId);
   }

   ImGui::EndDragDropTarget();
}

void EditorSceneSession::HandleViewportClickSelection(
   float viewportX, float viewportY, float viewportWidth, float viewportHeight) {
   if (viewportWidth <= 0.0f || viewportHeight <= 0.0f) return;

   if (!ImGui::IsWindowHovered() || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;

   if (context_->selectedObject_ && (ImGuizmo::IsOver() || ImGuizmo::IsUsing())) return;

   ImVec2 mouse = ImGui::GetMousePos();

   if (mouse.x < viewportX || mouse.x > viewportX + viewportWidth || mouse.y < viewportY ||
       mouse.y > viewportY + viewportHeight) {
      return;
   }

   Camera* camera = EngineContext::GetActiveCamera();

   if (!camera) return;

   Object* nearestObject = nullptr;
   float nearestDistanceSquared = std::numeric_limits<float>::max();
   constexpr float kMinPickRadiusPixels = 24.0f;
   constexpr float kMaxPickRadiusPixels = 220.0f;
   const Vector2 screenSize = GetEditorScreenCameraSize(viewportWidth, viewportHeight);
   const Vector2 screenLayoutSize = GetEditorScreenLayoutSize();
   const Matrix4x4 screenViewProjection = MakeScreenSpaceProjectionMatrix(screenSize);
   const Matrix4x4 uiTextScreenViewProjection = MakeScreenSpaceProjectionMatrix(screenSize, true);

   // 専用Pickingバッファを持たないため、各中心と投影後の概算半径を使って最も近い候補を選ぶ。
   for (Object* object : context_->CollectEditableObjects()) {
      if (!object) {
         continue;
      }

      const auto* transformComponent = object->GetComponent<TransformComponent>();

      if (!transformComponent) {
         continue;
      }

      const bool useScreenSpace = UsesScreenRenderSpace(object);
      const bool useUITextCoordinates = useScreenSpace && dynamic_cast<const UIText*>(object) != nullptr;
      const Vector3 screenRenderOffset =
         useScreenSpace ? GetScreenRenderOffset(object, screenLayoutSize) : Vector3(0.0f, 0.0f, 0.0f);
      const Matrix4x4 viewProjection = useScreenSpace
                                          ? (useUITextCoordinates ? uiTextScreenViewProjection : screenViewProjection)
                                          : camera->GetViewProjectionMatrix();
      const Vector3 objectCenter =
         useScreenSpace
            ? ToEditorScreenWorldPosition(transformComponent->GetLocalPose().translation, screenRenderOffset)
            : transformComponent->GetLocalPose().translation;

      const Vector3 screenPosition =
         Project(objectCenter, viewportX, viewportY, viewportWidth, viewportHeight, viewProjection);

      if (screenPosition.z < 0.0f || screenPosition.z > 1.0f) {
         continue;
      }

      const Vector3 scale = transformComponent->GetLocalPose().scale;
      float worldRadius = std::max({std::abs(scale.x), std::abs(scale.y), std::abs(scale.z), 1.0f}) * 0.5f;

      if (const auto* sprite = dynamic_cast<const Sprite*>(object); sprite && useScreenSpace) {
         const Vector2 size = sprite->GetSize();
         worldRadius = std::max({std::abs(size.x * scale.x), std::abs(size.y * scale.y), 1.0f}) * 0.5f;
      } else if (const auto* uiText = dynamic_cast<const UIText*>(object); uiText && useScreenSpace) {
         if (const auto* textComponent = uiText->GetComponent<UITextComponent>()) {
            const Vector2 size = EngineContext::MeasureText(textComponent->GetText(), textComponent->GetStyle());
            worldRadius = std::max({std::abs(size.x * scale.x), std::abs(size.y * scale.y), 1.0f}) * 0.5f;
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
         const Vector3 sample =
            Project(objectCenter + offset, viewportX, viewportY, viewportWidth, viewportHeight, viewProjection);

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

   context_->SelectObject(nearestObject);
}

bool EditorSceneSession::HasTransformChanged(const Transform& lhs, const Transform& rhs) const {
   // 行列の分解誤差で空のギズモ操作が履歴化されないよう、編集精度より小さい差を無視する。
   constexpr float kEpsilon = 0.0001f;
   const Vector3 lhsRotation = lhs.GetActiveEuler();
   const Vector3 rhsRotation = rhs.GetActiveEuler();
   return AbsDiff(lhs.translation.x, rhs.translation.x) > kEpsilon ||
          AbsDiff(lhs.translation.y, rhs.translation.y) > kEpsilon ||
          AbsDiff(lhs.translation.z, rhs.translation.z) > kEpsilon ||
          AbsDiff(lhsRotation.x, rhsRotation.x) > kEpsilon || AbsDiff(lhsRotation.y, rhsRotation.y) > kEpsilon ||
          AbsDiff(lhsRotation.z, rhsRotation.z) > kEpsilon || AbsDiff(lhs.scale.x, rhs.scale.x) > kEpsilon ||
          AbsDiff(lhs.scale.y, rhs.scale.y) > kEpsilon || AbsDiff(lhs.scale.z, rhs.scale.z) > kEpsilon;
}

void EditorSceneSession::SubmitTransformIfNeeded(const Transform& before, const Transform& after, Object* object) {
   if (!object || !HasTransformChanged(before, after)) return;

   context_->CommitEdit(
      std::make_unique<TransformObjectCommand>(context_->GetObjectIdForCommand(object), before, after));
}

void EditorSceneSession::SubmitParticleTransformIfNeeded(
   const Transform& before, const Transform& after, ParticleSystem* particleSystem) {
   if (!particleSystem || !HasTransformChanged(before, after)) return;

   context_->CommitEdit(std::make_unique<TransformParticleSystemCommand>(
      context_->GetParticleSystemIdForCommand(particleSystem), before, after));
}

}
#endif
