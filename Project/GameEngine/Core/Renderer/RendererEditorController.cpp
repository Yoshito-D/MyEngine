#include "pch.h"
#include "RendererEditorController.h"

#ifdef USE_IMGUI

#include "Asset/AssetManager.h"
#include "Asset/MaterialManager.h"
#include "Asset/ModelAssetManager.h"
#include "Asset/AnimationAssetManager.h"
#include "Asset/TextureManager.h"
#include "Component/AnimationComponent.h"
#include "Component/RenderComponent.h"
#include "Model/Model.h"
#include "Sprite/Sprite.h"
#include "externals/imgui/imgui.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>

namespace GameEngine {

void RendererEditorController::Initialize(AssetManager* assetManager) {
   assetManager_ = assetManager;
}

void RendererEditorController::ShowSceneEditorWindow() {
   ImGui::Begin("Scene Editor");

   auto* modelManager = assetManager_ ? assetManager_->GetModelAssetManager() : nullptr;
   auto* materialManager = assetManager_ ? assetManager_->GetMaterialManager() : nullptr;

   const auto modelNames = modelManager ? modelManager->GetModelNames() : std::vector<std::string>{};
   const auto materialNames = materialManager ? materialManager->GetMaterialNames() : std::vector<std::string>{};

   if (!modelNames.empty()) {
      editorSelectedModelAssetIndex_ = std::clamp(editorSelectedModelAssetIndex_, 0, static_cast<int>(modelNames.size() - 1));
   } else {
      editorSelectedModelAssetIndex_ = 0;
   }

   if (!materialNames.empty()) {
      editorSelectedMaterialIndex_ = std::clamp(editorSelectedMaterialIndex_, 0, static_cast<int>(materialNames.size() - 1));
   } else {
      editorSelectedMaterialIndex_ = 0;
   }

   ImGui::Text("Create Object");
   ImGui::Separator();

   char modelNameBuffer[128]{};
   std::memcpy(modelNameBuffer, editorNewModelName_.c_str(), std::min(editorNewModelName_.size(), sizeof(modelNameBuffer) - 1));
   if (ImGui::InputText("New Model Name", modelNameBuffer, sizeof(modelNameBuffer))) {
      editorNewModelName_ = modelNameBuffer;
   }

   if (ImGui::BeginCombo("Model Asset", modelNames.empty() ? "<none>" : modelNames[editorSelectedModelAssetIndex_].c_str())) {
      for (size_t i = 0; i < modelNames.size(); ++i) {
         ImGui::PushID(static_cast<int>(i));
         const bool selected = (static_cast<int>(i) == editorSelectedModelAssetIndex_);
         if (ImGui::Selectable(modelNames[i].c_str(), selected)) {
            editorSelectedModelAssetIndex_ = static_cast<int>(i);
         }
         ImGui::PopID();
      }
      ImGui::EndCombo();
   }

   if (!modelNames.empty() && modelManager) {
      if (auto previewModel = modelManager->GetModel(modelNames[editorSelectedModelAssetIndex_])) {
         ImGui::Text("Model Preview");
         ImGui::Text("Name: %s", modelNames[editorSelectedModelAssetIndex_].c_str());
         ImGui::Text("Meshes: %zu", previewModel->GetMeshData().size());
         ImGui::Text("Materials: %zu", previewModel->GetMaterialAssets().size());
      }
   }

   if (ImGui::BeginCombo("Model Material", materialNames.empty() ? "<none>" : materialNames[editorSelectedMaterialIndex_].c_str())) {
      for (size_t i = 0; i < materialNames.size(); ++i) {
         ImGui::PushID(1000 + static_cast<int>(i));
         const bool selected = (static_cast<int>(i) == editorSelectedMaterialIndex_);
         if (ImGui::Selectable(materialNames[i].c_str(), selected)) {
            editorSelectedMaterialIndex_ = static_cast<int>(i);
         }
         ImGui::PopID();
      }
      ImGui::EndCombo();
   }

   if (ImGui::Button("Create Model") && modelManager && materialManager && !modelNames.empty() && !materialNames.empty()) {
      auto modelAsset = modelManager->GetModel(modelNames[editorSelectedModelAssetIndex_]);
      auto* material = materialManager->GetMaterial(materialNames[editorSelectedMaterialIndex_]);
      if (modelAsset && material) {
         auto model = std::make_unique<Model>();
         model->Create(modelAsset, material);

         const auto sceneObjects = CollectSceneObjects();
         model->SetObjectName(BuildUniqueObjectName(editorNewModelName_.empty() ? "Model" : editorNewModelName_, sceneObjects));

         if (auto* materialComponent = model->GetMaterialComponent()) {
            nlohmann::json data = nlohmann::json::object();
            data["materialNames"] = nlohmann::json::array({ materialNames[editorSelectedMaterialIndex_] });
            materialComponent->Deserialize(data);
         }

         editorModelAssetNames_[model.get()] = modelNames[editorSelectedModelAssetIndex_];
         editorModelMaterialNames_[model.get()] = materialNames[editorSelectedMaterialIndex_];
         selectedObject_ = model.get();
         editorCreatedModels_.push_back(std::move(model));
      }
   }

   ImGui::Spacing();

   char spriteNameBuffer[128]{};
   std::memcpy(spriteNameBuffer, editorNewSpriteName_.c_str(), std::min(editorNewSpriteName_.size(), sizeof(spriteNameBuffer) - 1));
   if (ImGui::InputText("New Sprite Name", spriteNameBuffer, sizeof(spriteNameBuffer))) {
      editorNewSpriteName_ = spriteNameBuffer;
   }

   if (ImGui::Button("Create Sprite") && materialManager) {
      auto sprite = std::make_unique<Sprite>();
      Material* material = nullptr;
      if (!materialNames.empty()) {
         material = materialManager->GetMaterial(materialNames[editorSelectedMaterialIndex_]);
      }
      sprite->Create(Vector2(128.0f, 128.0f), material);

      const auto sceneObjects = CollectSceneObjects();
      sprite->SetObjectName(BuildUniqueObjectName(editorNewSpriteName_.empty() ? "Sprite" : editorNewSpriteName_, sceneObjects));

      if (auto* materialComponent = sprite->GetMaterialComponent(); materialComponent && !materialNames.empty()) {
         nlohmann::json data = nlohmann::json::object();
         data["materialNames"] = nlohmann::json::array({ materialNames[editorSelectedMaterialIndex_] });
         materialComponent->Deserialize(data);
      }

      selectedObject_ = sprite.get();
      editorCreatedSprites_.push_back(std::move(sprite));
   }

   ImGui::Spacing();
   ImGui::Text("Scene File");
   ImGui::Separator();

   std::string scenePathString = editorSceneFilePath_.string();
   char scenePathBuffer[512]{};
   std::memcpy(scenePathBuffer, scenePathString.c_str(), std::min(scenePathString.size(), sizeof(scenePathBuffer) - 1));
   if (ImGui::InputText("Path", scenePathBuffer, sizeof(scenePathBuffer))) {
      editorSceneFilePath_ = scenePathBuffer;
   }

   if (ImGui::Button("Save Scene")) {
      SaveEditorSceneToFile(editorSceneFilePath_);
   }
   ImGui::SameLine();
   if (ImGui::Button("Load Scene")) {
      LoadEditorSceneFromFile(editorSceneFilePath_);
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

   auto* transformComponent = selectedObject_->GetTransformComponent();
   if (transformComponent && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::DragFloat3("Position", &transformComponent->transform.translation.x, 0.05f);
      ImGui::DragFloat3("Rotation", &transformComponent->transform.rotation.x, 0.01f);
      ImGui::DragFloat3("Scale", &transformComponent->transform.scale.x, 0.01f, 0.001f, 1000.0f);

      const auto sceneObjects = CollectSceneObjects();
      std::vector<std::string> parentCandidates;
      parentCandidates.reserve(sceneObjects.size() + 1);
      parentCandidates.push_back("<None>");
      for (auto* object : sceneObjects) {
         if (!object || object == selectedObject_) {
            continue;
         }
         parentCandidates.push_back(object->GetObjectName());
      }

      std::string currentParent = transformComponent->parentObjectName.empty() ? "<None>" : transformComponent->parentObjectName;
      if (ImGui::BeginCombo("Parent", currentParent.c_str())) {
         for (size_t i = 0; i < parentCandidates.size(); ++i) {
            const auto& candidate = parentCandidates[i];
            ImGui::PushID(2000 + static_cast<int>(i));
            const bool isSelected = (currentParent == candidate);
            if (ImGui::Selectable(candidate.c_str(), isSelected)) {
               if (candidate == "<None>") {
                  transformComponent->parentObjectName.clear();
                  transformComponent->useParentMatrix = false;
               } else {
                  transformComponent->parentObjectName = candidate;
                  transformComponent->useParentMatrix = true;
               }
            }
            ImGui::PopID();
         }
         ImGui::EndCombo();
      }
      ImGui::Spacing();
   }

   auto* renderComponent = selectedObject_->GetRenderComponent();
   if (renderComponent && ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("Visible", &renderComponent->visible);
      ImGui::Checkbox("Auto Render", &renderComponent->autoRender);
      ImGui::Checkbox("Apply PostProcess", &renderComponent->applyPostProcess);

      auto* textureManager = assetManager_ ? assetManager_->GetTextureManager() : nullptr;
      if (textureManager) {
         const auto textureNames = textureManager->GetTextureNames();
         if (!textureNames.empty()) {
            if (ImGui::BeginCombo("Texture", renderComponent->textureName.c_str())) {
               for (size_t i = 0; i < textureNames.size(); ++i) {
                  const auto& textureName = textureNames[i];
                  ImGui::PushID(3000 + static_cast<int>(i));
                  const bool isSelected = (renderComponent->textureName == textureName);
                  if (ImGui::Selectable(textureName.c_str(), isSelected)) {
                     renderComponent->textureName = textureName;
                  }
                  if (isSelected) {
                     ImGui::SetItemDefaultFocus();
                  }
                  ImGui::PopID();
               }
               ImGui::EndCombo();
            }

            if (auto* texture = textureManager->GetTexture(renderComponent->textureName)) {
               ImGui::Text("Texture Preview");
               ImTextureID texId = (ImTextureID)(texture->GetTextureSrvHandleGPU().ptr);
               ImGui::Image(texId, ImVec2(96.0f, 96.0f));
            }
         }
      }
      ImGui::Spacing();
   }

   auto* materialComponent = selectedObject_->GetMaterialComponent();
   if (materialComponent && !materialComponent->materials.empty() && materialComponent->materials[0] && ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
      auto* material = materialComponent->materials[0];
      auto* data = material->GetMaterialData();
      if (data) {
         Vector4 color = data->color;
         if (ImGui::ColorEdit4("Color", &color.x)) {
            material->SetColor(color);
         }

         const char* lightingModeLabels[] = { "None", "Lambert", "HalfLambert", "Phong", "BlinnPhong" };
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

         Vector2 uvScale = material->GetUVScale();
         float uvRotation = material->GetUVRotation();
         Vector2 uvTranslation = material->GetUVTranslation();
         bool changed = false;
         changed |= ImGui::DragFloat2("UV Scale", &uvScale.x, 0.01f);
         changed |= ImGui::DragFloat("UV Rotation", &uvRotation, 0.01f);
         changed |= ImGui::DragFloat2("UV Translation", &uvTranslation.x, 0.01f);
         if (changed) {
            material->SetUVTransform(uvScale, uvRotation, uvTranslation);
         }
      }
   }

   if (auto* model = dynamic_cast<Model*>(selectedObject_); model && ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen)) {
      auto itModelName = editorModelAssetNames_.find(model);
      if (itModelName != editorModelAssetNames_.end()) {
         ImGui::Text("Asset: %s", itModelName->second.c_str());
      }

      if (auto* modelAsset = model->GetModelAsset()) {
         ImGui::Text("Meshes: %zu", modelAsset->GetMeshData().size());
         ImGui::Text("Materials: %zu", modelAsset->GetMaterialAssets().size());
      }
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

   if (auto* animationComponent = selectedObject_->GetComponent<AnimationComponent>();
      animationComponent && ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("Playing", &animationComponent->playing);
      ImGui::Checkbox("Loop", &animationComponent->loop);
      ImGui::DragFloat("Playback Speed", &animationComponent->playbackSpeed, 0.01f, -4.0f, 4.0f);
      ImGui::Checkbox("Apply Translation", &animationComponent->applyTranslation);
      ImGui::Checkbox("Apply Rotation", &animationComponent->applyRotation);
      ImGui::Checkbox("Apply Scale", &animationComponent->applyScale);

      auto* animationManager = assetManager_ ? assetManager_->GetAnimationAssetManager() : nullptr;
      const auto animationNames = animationManager ? animationManager->GetAnimationNames() : std::vector<std::string>{};
      if (!animationNames.empty()) {
         const char* previewName = animationComponent->animationName.empty()
            ? animationNames.front().c_str()
            : animationComponent->animationName.c_str();

         if (ImGui::BeginCombo("Animation Asset", previewName)) {
            for (size_t i = 0; i < animationNames.size(); ++i) {
               const auto& name = animationNames[i];
               ImGui::PushID(5000 + static_cast<int>(i));
               const bool isSelected = (animationComponent->animationName == name);
               if (ImGui::Selectable(name.c_str(), isSelected)) {
                  animationComponent->animationName = name;
                  animationComponent->currentTime = 0.0f;
               }
               if (isSelected) {
                  ImGui::SetItemDefaultFocus();
               }
               ImGui::PopID();
            }
            ImGui::EndCombo();
         }
      }

      if (animationManager && !animationComponent->animationName.empty()) {
         auto animationAsset = animationManager->GetAnimation(animationComponent->animationName);
         if (animationAsset && animationAsset->HasAnyClip()) {
            const auto clipNames = animationAsset->GetClipNames();
            const std::string previewClip = animationComponent->clipName.empty()
               ? animationAsset->GetDefaultClipName()
               : animationComponent->clipName;

            if (ImGui::BeginCombo("Animation Clip", previewClip.c_str())) {
               for (size_t i = 0; i < clipNames.size(); ++i) {
                  const auto& name = clipNames[i];
                  ImGui::PushID(5400 + static_cast<int>(i));
                  const bool isSelected = (animationComponent->clipName == name);
                  if (ImGui::Selectable(name.c_str(), isSelected)) {
                     animationComponent->clipName = name;
                     animationComponent->currentTime = 0.0f;
                  }
                  if (isSelected) {
                     ImGui::SetItemDefaultFocus();
                  }
                  ImGui::PopID();
               }
               ImGui::EndCombo();
            }
         }
      }

      char targetNodeBuffer[256]{};
      std::memcpy(targetNodeBuffer, animationComponent->targetNodeName.c_str(), std::min(animationComponent->targetNodeName.size(), sizeof(targetNodeBuffer) - 1));
      if (ImGui::InputText("Target Node", targetNodeBuffer, sizeof(targetNodeBuffer))) {
         animationComponent->targetNodeName = targetNodeBuffer;
      }
      ImGui::DragFloat("Current Time", &animationComponent->currentTime, 0.01f, 0.0f, 1000.0f);
      ImGui::Spacing();
   }

   ImGui::End();
}

std::vector<Object*> RendererEditorController::CollectSceneObjects() const {
   std::vector<Object*> objects;

   const auto& models = Model::GetRegisteredModels();
   objects.reserve(models.size() + Sprite::GetRegisteredSprites().size());
   for (auto* model : models) {
      if (model) {
         objects.push_back(model);
      }
   }

   const auto& sprites = Sprite::GetRegisteredSprites();
   for (auto* sprite : sprites) {
      if (sprite) {
         objects.push_back(sprite);
      }
   }

   return objects;
}

void RendererEditorController::ResolveParentRelation(Object* object, const std::vector<Object*>& sceneObjects) const {
   if (!object) {
      return;
   }

   auto* transformComponent = object->GetTransformComponent();
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

   const auto* parentTransform = parentObject->GetTransformComponent();
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

      auto itModelName = editorModelAssetNames_.find(modelPtr.get());
      if (itModelName != editorModelAssetNames_.end()) {
         entry["modelName"] = itModelName->second;
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
         auto modelAsset = modelManager->GetModel(modelName);
         auto* material = materialManager->GetMaterial(materialName);
         if (!modelAsset || !material) {
            continue;
         }

         auto model = std::make_unique<Model>();
         model->Create(modelAsset, material);
         model->SetObjectName(name);
         if (objectJson.contains("components") && objectJson.at("components").is_array()) {
            model->DeserializeComponents(objectJson.at("components"));
         }
         editorModelAssetNames_[model.get()] = modelName;
         editorModelMaterialNames_[model.get()] = materialName;
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
