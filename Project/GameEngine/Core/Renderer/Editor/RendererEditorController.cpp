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
#include "externals/imgui/imgui.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>

namespace GameEngine {

void RendererEditorController::Initialize(AssetManager* assetManager) {
   assetManager_ = assetManager;

   if (!editorSceneFilePath_.empty() && std::filesystem::exists(editorSceneFilePath_)) {
      LoadEditorSceneFromFile(editorSceneFilePath_);
   }
}

void RendererEditorController::ShowAssetWindow() {
   ImGui::Begin("Assets");

   auto* materialManager = assetManager_ ? assetManager_->GetMaterialManager() : nullptr;
   if (!materialManager) {
      ImGui::Text("MaterialManager is not available");
      ImGui::End();
      return;
   }

   ImGui::Text("Material Assets");
   ImGui::Separator();

   char materialNameBuffer[128]{};
   std::memcpy(materialNameBuffer, editorNewMaterialName_.c_str(), std::min(editorNewMaterialName_.size(), sizeof(materialNameBuffer) - 1));
   if (ImGui::InputText("New Material Name", materialNameBuffer, sizeof(materialNameBuffer))) {
      editorNewMaterialName_ = materialNameBuffer;
   }

   ImGui::ColorEdit4("New Material Color", editorNewMaterialColor_);

   const char* lightingModeLabels[] = { "None", "Lambert", "HalfLambert", "Phong", "BlinnPhong" };
   editorNewMaterialLightingMode_ = std::clamp(editorNewMaterialLightingMode_, 0, 4);
   if (ImGui::BeginCombo("New Material Lighting", lightingModeLabels[editorNewMaterialLightingMode_])) {
      for (int i = 0; i < 5; ++i) {
         const bool selected = (i == editorNewMaterialLightingMode_);
         if (ImGui::Selectable(lightingModeLabels[i], selected)) {
            editorNewMaterialLightingMode_ = i;
         }
         if (selected) {
            ImGui::SetItemDefaultFocus();
         }
      }
      ImGui::EndCombo();
   }

   if (ImGui::Button("Create Material Asset") && !editorNewMaterialName_.empty()) {
      auto* material = static_cast<Material*>(materialManager->CreateMaterial(editorNewMaterialName_));
      if (material) {
         material->SetColor(Vector4(
            editorNewMaterialColor_[0],
            editorNewMaterialColor_[1],
            editorNewMaterialColor_[2],
            editorNewMaterialColor_[3]));
         material->SetLightingMode(static_cast<Material::LightingMode>(editorNewMaterialLightingMode_));
      }
   }

   ImGui::Spacing();

   const auto materialNames = materialManager->GetMaterialNames();
   if (materialNames.empty()) {
      ImGui::Text("No material assets");
      ImGui::End();
      return;
   }

   editorSelectedAssetMaterialIndex_ = std::clamp(editorSelectedAssetMaterialIndex_, 0, static_cast<int>(materialNames.size() - 1));
   if (ImGui::BeginCombo("Material Asset", materialNames[editorSelectedAssetMaterialIndex_].c_str())) {
      for (size_t i = 0; i < materialNames.size(); ++i) {
         const bool selected = (static_cast<int>(i) == editorSelectedAssetMaterialIndex_);
         if (ImGui::Selectable(materialNames[i].c_str(), selected)) {
            editorSelectedAssetMaterialIndex_ = static_cast<int>(i);
         }
         if (selected) {
            ImGui::SetItemDefaultFocus();
         }
      }
      ImGui::EndCombo();
   }

   auto* material = materialManager->GetMaterial(materialNames[editorSelectedAssetMaterialIndex_]);
   if (material && material->GetMaterialData()) {
      auto* data = material->GetMaterialData();

      Vector4 color = data->color;
      if (ImGui::ColorEdit4("Color", &color.x)) {
         material->SetColor(color);
      }

      int lightingMode = std::clamp(data->lightingMode, 0, 4);
      if (ImGui::BeginCombo("Lighting Mode", lightingModeLabels[lightingMode])) {
         for (int i = 0; i < 5; ++i) {
            const bool selected = (i == lightingMode);
            if (ImGui::Selectable(lightingModeLabels[i], selected)) {
               material->SetLightingMode(static_cast<Material::LightingMode>(i));
            }
            if (selected) {
               ImGui::SetItemDefaultFocus();
            }
         }
         ImGui::EndCombo();
      }

      float shininess = data->shininess;
      if (ImGui::DragFloat("Shininess", &shininess, 0.1f, 0.0f, 256.0f)) {
         material->SetShininess(shininess);
      }
   }

   ImGui::End();
}

void RendererEditorController::ShowHierarchyWindow() {
   ImGui::Begin("Hierarchy");

   const auto sceneObjects = CollectSceneObjects();
   const auto& particleSystems = ParticleSystem::GetRegisteredParticleSystems();
   if (sceneObjects.empty() && particleSystems.empty()) {
      ImGui::Text("No objects");
      selectedObject_ = nullptr;
      selectedParticleSystem_ = nullptr;
      ImGui::End();
      return;
   }

   if (selectedObject_) {
      if (std::find(sceneObjects.begin(), sceneObjects.end(), selectedObject_) == sceneObjects.end()) {
         selectedObject_ = nullptr;
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

      const bool isSelected = (selectedObject_ == object);
      if (ImGui::Selectable(label.c_str(), isSelected)) {
         selectedObject_ = object;
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
         selectedObject_ = nullptr;
         selectedParticleSystem_ = particleSystem;
      }
      ImGui::PopID();
   }

   ImGui::End();
}

void RendererEditorController::ShowInspectorWindow() {
   ImGui::Begin("Inspector");

   if (!selectedObject_ && !selectedParticleSystem_) {
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
      ImGui::Spacing();

      ParticleSystemEdit::Edit(selectedParticleSystem_);
      ImGui::End();
      return;
   }

   std::string objectName = selectedObject_->GetObjectName();
   char objectNameBuffer[256]{};
   {
      const size_t copySize = std::min(objectName.size(), sizeof(objectNameBuffer) - 1);
      std::memcpy(objectNameBuffer, objectName.c_str(), copySize);
   }
   if (ImGui::InputText("Name", objectNameBuffer, sizeof(objectNameBuffer))) {
      selectedObject_->SetObjectName(objectNameBuffer);
   }
   ImGui::Spacing();

   selectedObject_->DrawComponentInspector();

   ImGui::Spacing();
   ImGui::PushID("InspectorAddComponent");
   if (ImGui::CollapsingHeader("Add Component##Header", ImGuiTreeNodeFlags_DefaultOpen)) {
      std::vector<std::string> addableComponentTypeNames;
      const auto registeredTypeNames = ComponentRegistry::GetInstance().GetRegisteredTypeNames();
      addableComponentTypeNames.reserve(registeredTypeNames.size());
      for (const auto& typeName : registeredTypeNames) {
         if (!selectedObject_->HasComponentByTypeName(typeName)) {
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
            selectedObject_->AddComponentByTypeName(addableComponentTypeNames[editorSelectedAddComponentIndex_]);
         }
      }
   }
   ImGui::PopID();

   if (auto* model = dynamic_cast<Model*>(selectedObject_); model && ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen)) {
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

      if (auto* modelAsset = model->GetModelAsset()) {
         ImGui::Text("Meshes: %zu", modelAsset->GetMeshData().size());
         ImGui::Text("Materials: %zu", modelAsset->GetMaterialAssets().size());
      }

      ImGui::TextDisabled("Texture は MaterialComponent で設定してください");
   }

   if (auto* sprite = dynamic_cast<Sprite*>(selectedObject_); sprite && ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen)) {
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
