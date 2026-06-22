#include "pch.h"
#include "RendererEditorController.h"

#ifdef USE_IMGUI

#include "Asset/AssetManager.h"
#include "Asset/MaterialManager.h"
#include "Asset/ModelAssetManager.h"
#include "Component/MaterialComponent.h"
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Graphics/Material.h"
#include "Component/ComponentRegistry.h"
#include "Model/Model.h"
#include "Sprite/Sprite.h"
#include "Object/Skybox/Skybox.h"
#include "Effect/ParticleSystem.h"
#include "Effect/ParticleSystemEdit.h"
#include "Editor/EditorSceneContext.h"
#include "Scene/BaseScene.h"
#include "externals/imgui/imgui.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>

namespace GameEngine {

void RendererEditorController::Initialize(AssetManager* assetManager) {
   assetManager_ = assetManager;
}

void RendererEditorController::ShowAssetWindow() {
   ImGui::Begin("Assets");

   auto* editorContext = GetActiveEditorContext();
   if (editorContext) {
      ImGui::Text("Scene");
      ImGui::Separator();
      if (ImGui::Button("Save Scene")) {
         editorContext->Save();
      }
      ImGui::SameLine();
      if (ImGui::Button("Reload Scene")) {
         editorContext->Load();
      }
      ImGui::SameLine();
      if (ImGui::Button("Rescan Assets")) {
         editorContext->GetAssetRegistry().Scan();
      }
      ImGui::TextDisabled("%s", editorContext->GetSceneFilePath().generic_string().c_str());
      ImGui::Spacing();

      ImGui::Text("Model Assets");
      ImGui::Separator();
      const auto& modelAssets = editorContext->GetAssetRegistry().GetModelAssets();
      if (modelAssets.empty()) {
         ImGui::Text("No .obj or .gltf models found");
      } else {
         for (const auto& entry : modelAssets) {
            ImGui::PushID(entry.assetId.c_str());
            if (ImGui::Selectable(entry.displayName.c_str(), false)) {
               editorContext->CreateModelFromAsset(entry.assetId);
            }
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
               ImGui::SetDragDropPayload("EDITOR_MODEL_ASSET", entry.assetId.c_str(), entry.assetId.size() + 1);
               ImGui::Text("%s", entry.displayName.c_str());
               ImGui::EndDragDropSource();
            }
            ImGui::PopID();
         }
      }
      ImGui::Spacing();
   } else {
      ImGui::Text("Editor scene context is not available");
      ImGui::Spacing();
   }

   ImGui::End();
}

void RendererEditorController::ShowHierarchyWindow() {
   ImGui::Begin("Hierarchy");

   auto* editorContext = GetActiveEditorContext();
   const auto sceneObjects = editorContext ? editorContext->CollectEditableObjects() : CollectSceneObjects();
   const auto particleSystems = editorContext ? editorContext->CollectEditableParticleSystems() : ParticleSystem::GetRegisteredParticleSystems();

   if (editorContext && ImGui::BeginPopupContextWindow("HierarchyCreateContext", ImGuiPopupFlags_MouseButtonRight)) {
      if (ImGui::BeginMenu("Model")) {
         const auto& modelAssets = editorContext->GetAssetRegistry().GetModelAssets();
         if (modelAssets.empty()) {
            ImGui::TextDisabled("No .obj or .gltf models");
         } else {
            for (const auto& entry : modelAssets) {
               if (ImGui::MenuItem(entry.displayName.c_str())) {
                  editorContext->CreateModelFromAsset(entry.assetId);
                  selectedParticleSystem_ = nullptr;
               }
            }
         }
         ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Particle System")) {
         const auto& particleAssets = editorContext->GetAssetRegistry().GetParticleAssets();
         if (particleAssets.empty()) {
            ImGui::TextDisabled("No particle json files");
         } else {
            for (const auto& entry : particleAssets) {
               if (ImGui::MenuItem(entry.displayName.c_str())) {
                  selectedParticleSystem_ = editorContext->CreateParticleSystemFromAsset(entry.assetId);
                  editorContext->SelectObject(nullptr);
               }
            }
         }
         ImGui::EndMenu();
      }

      ImGui::EndPopup();
   }

   if (sceneObjects.empty() && particleSystems.empty()) {
      ImGui::Text("No objects");
      if (editorContext) {
         editorContext->SelectObject(nullptr);
      }
      selectedParticleSystem_ = nullptr;
      ImGui::End();
      return;
   }

   Object* selectedObject = editorContext ? editorContext->GetSelectedObject() : nullptr;
   if (selectedObject) {
      if (std::find(sceneObjects.begin(), sceneObjects.end(), selectedObject) == sceneObjects.end()) {
         if (editorContext) {
            editorContext->SelectObject(nullptr);
         }
         selectedObject = nullptr;
      }
   }

   if (selectedParticleSystem_) {
      if (std::find(particleSystems.begin(), particleSystems.end(), selectedParticleSystem_) == particleSystems.end()) {
         selectedParticleSystem_ = nullptr;
      }
   }

   for (size_t i = 0; i < sceneObjects.size(); ++i) {
      Object* object = sceneObjects[i];
      if (!object) {
         continue;
      }

      ImGui::PushID(static_cast<int>(i));

      std::string label = object->GetObjectName();
      label += "##Object_" + std::to_string(i);

      const bool isSelected = (selectedObject == object);
      if (ImGui::Selectable(label.c_str(), isSelected)) {
         if (editorContext) {
            editorContext->SelectObject(object);
         }
         selectedParticleSystem_ = nullptr;
      }
      ImGui::PopID();
   }

   for (size_t i = 0; i < particleSystems.size(); ++i) {
      auto* particleSystem = particleSystems[i];
      if (!particleSystem) {
         continue;
      }

      ImGui::PushID(static_cast<int>(sceneObjects.size() + i));

      std::string label = particleSystem->GetName();
      label += "##ParticleSystem_" + std::to_string(i);

      const bool isSelected = (selectedParticleSystem_ == particleSystem);
      if (ImGui::Selectable(label.c_str(), isSelected)) {
         if (editorContext) {
            editorContext->SelectObject(nullptr);
         }
         selectedParticleSystem_ = particleSystem;
      }
      ImGui::PopID();
   }

   ImGui::End();
}

void RendererEditorController::ShowInspectorWindow() {
   ImGui::Begin("Inspector");

   auto* editorContext = GetActiveEditorContext();
   Object* selectedObject = editorContext ? editorContext->GetSelectedObject() : nullptr;

   if (editorContext) {

      const char* operationLabels[] = { "Move", "Rotate", "Scale" };
      int operation = static_cast<int>(editorContext->GetGizmoOperation());
      if (ImGui::Combo("Gizmo Operation", &operation, operationLabels, 3)) {
         editorContext->SetGizmoOperation(static_cast<EditorSceneContext::GizmoOperation>(operation));
      }

      const char* modeLabels[] = { "Local", "World" };
      int mode = static_cast<int>(editorContext->GetGizmoMode());
      if (ImGui::Combo("Gizmo Mode", &mode, modeLabels, 2)) {
         editorContext->SetGizmoMode(static_cast<EditorSceneContext::GizmoMode>(mode));
      }

      ImGui::Spacing();
   }

   if (!selectedObject && !selectedParticleSystem_) {
      ImGui::Text("No selection");
      ImGui::End();
      return;
   }

   if (selectedParticleSystem_) {
      std::string particleSystemName = selectedParticleSystem_->GetName();
      char particleSystemNameBuffer[256]{};
      {
         const size_t copySize = std::min(particleSystemName.size(), sizeof(particleSystemNameBuffer) - 1);
         std::memcpy(particleSystemNameBuffer, particleSystemName.c_str(), copySize);
      }
      if (ImGui::InputText("Name", particleSystemNameBuffer, sizeof(particleSystemNameBuffer))) {
         selectedParticleSystem_->SetName(particleSystemNameBuffer);
      }
      if (editorContext && editorContext->CanDeleteParticleSystem(selectedParticleSystem_)) {
         if (ImGui::Button("Delete")) {
            editorContext->DeleteParticleSystem(selectedParticleSystem_);
            selectedParticleSystem_ = nullptr;
            ImGui::End();
            return;
         }
      }
      ImGui::Spacing();

      ParticleSystemEdit::Edit(selectedParticleSystem_);
      ImGui::End();
      return;
   }

   std::string objectName = selectedObject->GetObjectName();
   char objectNameBuffer[256]{};
   {
      const size_t copySize = std::min(objectName.size(), sizeof(objectNameBuffer) - 1);
      std::memcpy(objectNameBuffer, objectName.c_str(), copySize);
   }
   if (ImGui::InputText("Name", objectNameBuffer, sizeof(objectNameBuffer))) {
      selectedObject->SetObjectName(objectNameBuffer);
   }
   ImGui::Spacing();

   if (editorContext) {
      const bool editorOwned = editorContext->IsEditorOwned(selectedObject);
      ImGui::Text("Owner: %s", editorOwned ? "Editor" : "Scene");
      if (editorContext->CanDeleteObject(selectedObject)) {
         if (ImGui::Button("Delete")) {
            editorContext->DeleteSelectedObject();
            ImGui::End();
            return;
         }
      }
      ImGui::Spacing();
   }

   selectedObject->DrawComponentInspector();

   ImGui::Spacing();
   ImGui::PushID("InspectorAddComponent");
   if (ImGui::CollapsingHeader("Add Component##Header", ImGuiTreeNodeFlags_DefaultOpen)) {
      std::vector<std::string> addableComponentTypeNames;
      const auto registeredTypeNames = ComponentRegistry::GetInstance().GetRegisteredTypeNames();
      addableComponentTypeNames.reserve(registeredTypeNames.size());
      for (const auto& typeName : registeredTypeNames) {
         if (!selectedObject->HasComponentByTypeName(typeName)) {
            addableComponentTypeNames.push_back(typeName);
         }
      }

      if (addableComponentTypeNames.empty()) {
         ImGui::Text("No addable components");
      } else {
         editorSelectedAddComponentIndex_ = std::clamp(editorSelectedAddComponentIndex_, 0, static_cast<int>(addableComponentTypeNames.size() - 1));
         const char* selectedTypeName = addableComponentTypeNames[editorSelectedAddComponentIndex_].c_str();

         if (ImGui::BeginCombo("Type##ComponentType", selectedTypeName)) {
            for (size_t i = 0; i < addableComponentTypeNames.size(); ++i) {
               const bool selected = (static_cast<int>(i) == editorSelectedAddComponentIndex_);
               if (ImGui::Selectable(addableComponentTypeNames[i].c_str(), selected)) {
                  editorSelectedAddComponentIndex_ = static_cast<int>(i);
               }
            }
            ImGui::EndCombo();
         }

         if (ImGui::Button("Add Component##Button")) {
            selectedObject->AddComponentByTypeName(addableComponentTypeNames[editorSelectedAddComponentIndex_]);
         }
      }
   }
   ImGui::PopID();

   if (auto* model = dynamic_cast<Model*>(selectedObject); model && ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen)) {
      auto* modelManager = assetManager_ ? assetManager_->GetModelAssetManager() : nullptr;
      const auto modelNames = modelManager ? modelManager->GetModelNames() : std::vector<std::string>{};

      auto itModelName = editorModelAssetNames_.find(model);
      if (itModelName != editorModelAssetNames_.end()) {
         ImGui::Text("Asset: %s", itModelName->second.c_str());
      }

      if (modelManager && !modelNames.empty()) {
         int selectedIndex = 0;
         if (itModelName != editorModelAssetNames_.end()) {
            for (size_t i = 0; i < modelNames.size(); ++i) {
               if (modelNames[i] == itModelName->second) {
                  selectedIndex = static_cast<int>(i);
                  break;
               }
            }
         }

         if (ImGui::BeginCombo("Model Asset##InspectorModelAsset", modelNames[selectedIndex].c_str())) {
            for (size_t i = 0; i < modelNames.size(); ++i) {
               const bool selected = (static_cast<int>(i) == selectedIndex);
               if (ImGui::Selectable(modelNames[i].c_str(), selected)) {
                  if (auto selectedAsset = modelManager->GetModel(modelNames[i])) {
                     model->SetModelAsset(selectedAsset);
                     editorModelAssetNames_[model] = modelNames[i];
                  }
               }
               if (selected) {
                  ImGui::SetItemDefaultFocus();
               }
            }
            ImGui::EndCombo();
         }
      }

      if (auto* modelAsset = model->GetComponent<ModelAssetComponent>()->GetModelAsset()) {
         ImGui::Text("Meshes: %zu", modelAsset->GetMeshData().size());
         ImGui::Text("Materials: %zu", modelAsset->GetMaterialAssets().size());
      }

      ImGui::TextDisabled("Texture は MaterialComponent で設定してください");
   }

   if (auto* sprite = dynamic_cast<Sprite*>(selectedObject); sprite && ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen)) {
      Vector2 size = sprite->GetSize();
      if (ImGui::DragFloat2("Size", &size.x, 0.1f, 0.0f, 4096.0f)) {
         sprite->SetSize(size);
      }
      Vector2 scale = sprite->GetScale();
      if (ImGui::DragFloat2("Scale", &scale.x, 0.01f, 0.001f, 100.0f)) {
         sprite->SetScale(scale);
      }
      float rotation = sprite->GetRotation();
      if (ImGui::DragFloat("Rotation Z", &rotation, 0.01f)) {
         sprite->SetRotation(rotation);
      }
      bool flipX = sprite->IsFlipX();
      bool flipY = sprite->IsFlipY();
      if (ImGui::Checkbox("Flip X", &flipX)) {
         sprite->SetFlipX(flipX);
      }
      if (ImGui::Checkbox("Flip Y", &flipY)) {
         sprite->SetFlipY(flipY);
      }

      ImGui::Spacing();
   }

   ImGui::End();
}

void RendererEditorController::ShowSceneOverlay(float viewportX, float viewportY, float viewportWidth, float viewportHeight) {
   auto* editorContext = GetActiveEditorContext();
   if (!editorContext) {
      return;
   }

   editorContext->HandleEditorShortcuts();
   editorContext->AcceptModelAssetDrop();
   editorContext->DrawTransformGizmo(viewportX, viewportY, viewportWidth, viewportHeight);
   editorContext->HandleViewportClickSelection(viewportX, viewportY, viewportWidth, viewportHeight);
}

EditorSceneContext* RendererEditorController::GetActiveEditorContext() const {
   auto* currentScene = BaseScene::GetCurrentScene();
   if (!currentScene) {
      return nullptr;
   }
   return currentScene->GetEditorSceneContext();
}

std::vector<Object*> RendererEditorController::CollectSceneObjects() const {
   std::vector<Object*> objects;

   const auto& models = Model::GetRegisteredModels();
   const auto& sprites = Sprite::GetRegisteredSprites();
   const auto& skyboxes = Skybox::GetRegisteredSkyboxes();
   objects.reserve(models.size() + sprites.size() + skyboxes.size());
   for (auto* model : models) {
      if (model) {
         objects.push_back(model);
      }
   }

   for (auto* sprite : sprites) {
      if (sprite) {
         objects.push_back(sprite);
      }
   }

   for (auto* skybox : skyboxes) {
      if (skybox) {
         objects.push_back(skybox);
      }
   }

   return objects;
}

void RendererEditorController::ResolveParentRelation(Object* object, const std::vector<Object*>& sceneObjects) const {
   if (!object) {
      return;
   }

   auto* transformComponent = object->GetComponent<TransformComponent>();
   if (!transformComponent) {
      return;
   }

   if (transformComponent->parentObjectName.empty()) {
      transformComponent->useParentMatrix = false;
      transformComponent->parentMatrix = MakeIdentity4x4();
      return;
   }

   Object* parentObject = nullptr;
   for (auto* candidate : sceneObjects) {
      if (!candidate || candidate == object) {
         continue;
      }
      if (candidate->GetObjectName() == transformComponent->parentObjectName) {
         parentObject = candidate;
         break;
      }
   }

   if (!parentObject) {
      transformComponent->useParentMatrix = false;
      transformComponent->parentMatrix = MakeIdentity4x4();
      return;
   }

   const auto* parentTransform = parentObject->GetComponent<TransformComponent>();
   if (!parentTransform) {
      transformComponent->useParentMatrix = false;
      transformComponent->parentMatrix = MakeIdentity4x4();
      return;
   }

   transformComponent->useParentMatrix = true;
   transformComponent->parentMatrix = MakeAffineMatrix(parentTransform->transform);
}

std::string RendererEditorController::BuildUniqueObjectName(const std::string& baseName, const std::vector<Object*>& sceneObjects) const {
   std::string candidate = baseName.empty() ? "Object" : baseName;
   auto exists = [&sceneObjects](const std::string& name) {
      for (auto* object : sceneObjects) {
         if (!object) {
            continue;
         }
         if (object->GetObjectName() == name) {
            return true;
         }
      }
      return false;
   };

   if (!exists(candidate)) {
      return candidate;
   }

   int index = 1;
   while (true) {
      std::string withIndex = candidate + "_" + std::to_string(index++);
      if (!exists(withIndex)) {
         return withIndex;
      }
   }
}

bool RendererEditorController::SaveEditorSceneToFile(const std::filesystem::path& filePath) const {
   (void)filePath;
   return false;
}

bool RendererEditorController::LoadEditorSceneFromFile(const std::filesystem::path& filePath) {
   (void)filePath;
   return false;
}

} // namespace GameEngine

#endif
