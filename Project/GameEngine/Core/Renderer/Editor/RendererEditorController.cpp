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
   if (sceneObjects.empty()) {
      ImGui::Text("No objects");
      selectedObject_ = nullptr;
      ImGui::End();
      return;
   }

   if (selectedObject_) {
      if (std::find(sceneObjects.begin(), sceneObjects.end(), selectedObject_) == sceneObjects.end()) {
         selectedObject_ = nullptr;
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
      }
      ImGui::PopID();
   }

   ImGui::End();
}

void RendererEditorController::ShowInspectorWindow() {
   ImGui::Begin("Inspector");

   if (!selectedObject_) {
      ImGui::Text("No selection");
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
   nlohmann::json sceneJson = nlohmann::json::object();
   sceneJson["objects"] = nlohmann::json::array();

   for (const auto& modelPtr : editorCreatedModels_) {
      if (!modelPtr) {
         continue;
      }

      nlohmann::json entry = nlohmann::json::object();
      entry["type"] = "Model";
      entry["name"] = modelPtr->GetObjectName();
      entry["components"] = modelPtr->SerializeComponents();

      auto* modelManager = assetManager_ ? assetManager_->GetModelAssetManager() : nullptr;
      auto* modelAsset = modelPtr->GetModelAsset();
      if (modelManager && modelAsset) {
         const auto modelNames = modelManager->GetModelNames();
         for (const auto& modelName : modelNames) {
            if (auto candidate = modelManager->GetModel(modelName); candidate && candidate.get() == modelAsset) {
               entry["modelName"] = modelName;
               break;
            }
         }
      }

      auto itModelName = editorModelAssetNames_.find(modelPtr.get());
      if (itModelName != editorModelAssetNames_.end()) {
         entry["modelName"] = itModelName->second;
      }

      if (const auto* materialComponent = modelPtr->GetComponent<MaterialComponent>()) {
         const auto& materialNames = materialComponent->GetMaterialNames();
         if (!materialNames.empty() && !materialNames[0].empty()) {
            entry["materialName"] = materialNames[0];
         }
      }

      auto itMaterialName = editorModelMaterialNames_.find(modelPtr.get());
      if (itMaterialName != editorModelMaterialNames_.end()) {
         entry["materialName"] = itMaterialName->second;
      }

      sceneJson["objects"].push_back(std::move(entry));
   }

   for (const auto& spritePtr : editorCreatedSprites_) {
      if (!spritePtr) {
         continue;
      }

      nlohmann::json entry = nlohmann::json::object();
      entry["type"] = "Sprite";
      entry["name"] = spritePtr->GetObjectName();
      entry["components"] = spritePtr->SerializeComponents();
      entry["sprite"] = {
         { "size", { spritePtr->GetSize().x, spritePtr->GetSize().y } },
         { "anchor", { spritePtr->GetAnchorPoint().x, spritePtr->GetAnchorPoint().y } },
         { "flipX", spritePtr->IsFlipX() },
         { "flipY", spritePtr->IsFlipY() }
      };

      sceneJson["objects"].push_back(std::move(entry));
   }

   std::ofstream ofs(filePath);
   if (!ofs.is_open()) {
      return false;
   }
   ofs << sceneJson.dump(2);
   return true;
}

bool RendererEditorController::LoadEditorSceneFromFile(const std::filesystem::path& filePath) {
   std::ifstream ifs(filePath);
   if (!ifs.is_open()) {
      return false;
   }

   nlohmann::json sceneJson;
   try {
      ifs >> sceneJson;
   } catch (...) {
      return false;
   }

   if (!sceneJson.contains("objects") || !sceneJson.at("objects").is_array()) {
      return false;
   }

   auto* modelManager = assetManager_ ? assetManager_->GetModelAssetManager() : nullptr;
   auto* materialManager = assetManager_ ? assetManager_->GetMaterialManager() : nullptr;
   if (!modelManager || !materialManager) {
      return false;
   }

   editorCreatedModels_.clear();
   editorCreatedSprites_.clear();
   editorModelAssetNames_.clear();
   editorModelMaterialNames_.clear();
   selectedObject_ = nullptr;

   for (const auto& objectJson : sceneJson.at("objects")) {
      if (!objectJson.is_object()) {
         continue;
      }

      const std::string type = objectJson.value("type", "");
      const std::string name = objectJson.value("name", "Object");

      if (type == "Model") {
         const std::string modelName = objectJson.value("modelName", "");
         const std::string materialName = objectJson.value("materialName", "");
         auto model = std::make_unique<Model>();
         model->Create();

         if (!modelName.empty()) {
            if (auto modelAsset = modelManager->GetModel(modelName)) {
               model->SetModelAsset(modelAsset);
               editorModelAssetNames_[model.get()] = modelName;
            }
         }

         if (!materialName.empty()) {
            if (auto* material = materialManager->GetMaterial(materialName)) {
               if (auto* materialComponent = model->GetComponent<MaterialComponent>()) {
                  materialComponent->AssignMaterial(material, materialName);
               }
               editorModelMaterialNames_[model.get()] = materialName;
            }
         }

         model->SetObjectName(name);
         if (objectJson.contains("components") && objectJson.at("components").is_array()) {
            model->DeserializeComponents(objectJson.at("components"));
         }

         if (const auto* materialComponent = model->GetComponent<MaterialComponent>()) {
            const auto& materialNames = materialComponent->GetMaterialNames();
            if (!materialNames.empty() && !materialNames[0].empty()) {
               editorModelMaterialNames_[model.get()] = materialNames[0];
            }
         }

         selectedObject_ = model.get();
         editorCreatedModels_.push_back(std::move(model));
      } else if (type == "Sprite") {
         auto sprite = std::make_unique<Sprite>();
         sprite->Create(Vector2(128.0f, 128.0f));
         sprite->SetObjectName(name);

         if (objectJson.contains("components") && objectJson.at("components").is_array()) {
            sprite->DeserializeComponents(objectJson.at("components"));
         }

         if (objectJson.contains("sprite") && objectJson.at("sprite").is_object()) {
            const auto& spriteJson = objectJson.at("sprite");
            if (spriteJson.contains("size") && spriteJson.at("size").is_array() && spriteJson.at("size").size() == 2) {
               sprite->SetSize(Vector2(spriteJson.at("size")[0].get<float>(), spriteJson.at("size")[1].get<float>()));
            }
            if (spriteJson.contains("anchor") && spriteJson.at("anchor").is_array() && spriteJson.at("anchor").size() == 2) {
               sprite->SetAnchorPoint(Vector2(spriteJson.at("anchor")[0].get<float>(), spriteJson.at("anchor")[1].get<float>()));
            }
            if (spriteJson.contains("flipX") && spriteJson.at("flipX").is_boolean()) {
               sprite->SetFlipX(spriteJson.at("flipX").get<bool>());
            }
            if (spriteJson.contains("flipY") && spriteJson.at("flipY").is_boolean()) {
               sprite->SetFlipY(spriteJson.at("flipY").get<bool>());
            }
         }

         selectedObject_ = sprite.get();
         editorCreatedSprites_.push_back(std::move(sprite));
      }
   }

   return true;
}

} // namespace GameEngine

#endif
