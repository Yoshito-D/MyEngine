#include "pch.h"
#include "RendererEditorController.h"

#ifdef USE_IMGUI

#include "Asset/AssetManager.h"
#include "Asset/MaterialManager.h"
#include "Asset/ModelAssetManager.h"
#include "Component/MaterialComponent.h"
#include "Component/ModelAssetComponent.h"
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Graphics/Material.h"
#include "Component/ComponentRegistry.h"
#include "Model/Model.h"
#include "Sprite/Sprite.h"
#include "Object/Skybox/Skybox.h"
#include "Effect/ParticleSystem.h"
#include "Effect/ParticleSystemEdit.h"
#include "Editor/EditorAssetRegistry.h"
#include "Editor/EditorSceneContext.h"
#include "Framework/EngineContext.h"
#include "Graphics/Texture.h"
#include "Scene/BaseScene.h"
#include "imgui.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <functional>
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace GameEngine {

namespace {
void PopLastUtf8Codepoint(std::string& text) {
   if (text.empty()) {
      return;
   }

   size_t erasePos = text.size() - 1;
   while (erasePos > 0) {
      const unsigned char c = static_cast<unsigned char>(text[erasePos]);
      if ((c & 0xC0) != 0x80) {
         break;
      }
      --erasePos;
   }
   text.erase(erasePos);
}

std::string TruncateTextWithEllipsis(const std::string& text, float maxWidth) {
   if (text.empty() || maxWidth <= 0.0f) {
      return {};
   }

   if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth) {
      return text;
   }

   constexpr const char* kEllipsis = "...";
   if (ImGui::CalcTextSize(kEllipsis).x >= maxWidth) {
      return kEllipsis;
   }

   std::string truncated = text;
   while (!truncated.empty()) {
      std::string candidate = truncated + kEllipsis;
      if (ImGui::CalcTextSize(candidate.c_str()).x <= maxWidth) {
         return candidate;
      }
      PopLastUtf8Codepoint(truncated);
   }

   return kEllipsis;
}
} // namespace

void RendererEditorController::Initialize(AssetManager* assetManager) {
   assetManager_ = assetManager;
}

void RendererEditorController::BeginEditorFrame() {
   auto* editorContext = GetActiveEditorContext();
   if (!editorContext) {
      return;
   }

   editorContext->GetObjectStore().FlushDeferredDeletes();
   editorContext->HandleEditorShortcuts();
}

void RendererEditorController::ShowAssetWindow() {
   ImGui::Begin("Assets");

   auto* editorContext = GetActiveEditorContext();
   if (editorContext) {
      const std::string dirtyMark = editorContext->IsDirty() ? " *" : "";
      ImGui::Text("Scene%s", dirtyMark.c_str());
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
      if (!editorContext->GetLastStatusMessage().empty()) {
         ImGui::TextWrapped("%s", editorContext->GetLastStatusMessage().c_str());
      }
      ImGui::Spacing();

      ImGui::Text("Project");
      ImGui::Separator();
      ImGui::Checkbox("Icon View", &editorAssetIconView_);
      if (editorContext->GetAssetRegistry().GetAllAssets().empty()) {
         ImGui::Text("No assets found under resources");
      } else {
         DrawAssetTree(*editorContext);
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
      if (ImGui::MenuItem("Duplicate Selected", "Ctrl+D")) {
         editorContext->DuplicateSelectedObject();
      }
      if (ImGui::MenuItem("Delete Selected", "Delete")) {
         editorContext->DeleteSelection();
      }
      ImGui::Separator();

      if (ImGui::BeginMenu("Model")) {
         const auto& modelAssets = editorContext->GetAssetRegistry().GetModelAssets();
         if (modelAssets.empty()) {
            ImGui::TextDisabled("No .obj or .gltf models");
         } else {
            for (const auto& entry : modelAssets) {
               if (ImGui::MenuItem(entry.displayName.c_str())) {
                  editorContext->CreateModelFromAsset(entry.assetId);
               }
            }
         }
         ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Sprite")) {
         const auto& textureAssets = editorContext->GetAssetRegistry().GetTextureAssets();
         if (textureAssets.empty()) {
            ImGui::TextDisabled("No texture files");
         } else {
            for (const auto& entry : textureAssets) {
               if (ImGui::MenuItem(entry.displayName.c_str())) {
                  editorContext->CreateSpriteFromTexture(entry.assetId);
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
                  editorContext->CreateParticleSystemFromAsset(entry.assetId);
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
         editorContext->SelectParticleSystem(nullptr);
      }
      ImGui::End();
      return;
   }

   Object* selectedObject = editorContext ? editorContext->GetSelectedObject() : nullptr;
   ParticleSystem* selectedParticleSystem = editorContext ? editorContext->GetSelectedParticleSystem() : nullptr;
   if (selectedObject) {
      if (std::find(sceneObjects.begin(), sceneObjects.end(), selectedObject) == sceneObjects.end()) {
         if (editorContext) {
            editorContext->SelectObject(nullptr);
         }
         selectedObject = nullptr;
      }
   }

   if (selectedParticleSystem) {
      if (std::find(particleSystems.begin(), particleSystems.end(), selectedParticleSystem) == particleSystems.end()) {
         if (editorContext) {
            editorContext->SelectParticleSystem(nullptr);
         }
         selectedParticleSystem = nullptr;
      }
   }

   ImGui::SetNextItemOpen(true, ImGuiCond_Once);
   if (ImGui::TreeNodeEx("Scene Objects", ImGuiTreeNodeFlags_DefaultOpen)) {
   for (size_t i = 0; i < sceneObjects.size(); ++i) {
      Object* object = sceneObjects[i];
      if (!object) {
         continue;
      }

      ImGui::PushID(static_cast<int>(i));

      const std::string objectName = object->GetObjectName();
      std::string label = objectName;
      label += "##Object_" + std::to_string(i);

      const bool isSelected = (selectedObject == object);
      ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
      if (isSelected) {
         flags |= ImGuiTreeNodeFlags_Selected;
      }
      ImGui::TreeNodeEx(label.c_str(), flags);
      const bool clicked = ImGui::IsItemClicked();
      if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
         ImGui::SetDragDropPayload("EDITOR_SCENE_OBJECT", objectName.c_str(), objectName.size() + 1);
         ImGui::Text("%s", objectName.c_str());
         ImGui::EndDragDropSource();
      }
      if (clicked) {
         if (editorContext) {
            editorContext->SelectObject(object);
         }
      }
      ImGui::PopID();
   }
      ImGui::TreePop();
   }

   ImGui::SetNextItemOpen(true, ImGuiCond_Once);
   if (ImGui::TreeNodeEx("Particle Systems", ImGuiTreeNodeFlags_DefaultOpen)) {
   for (size_t i = 0; i < particleSystems.size(); ++i) {
      auto* particleSystem = particleSystems[i];
      if (!particleSystem) {
         continue;
      }

      ImGui::PushID(static_cast<int>(sceneObjects.size() + i));

      std::string label = particleSystem->GetName();
      label += "##ParticleSystem_" + std::to_string(i);

      const bool isSelected = (selectedParticleSystem == particleSystem);
      ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
      if (isSelected) {
         flags |= ImGuiTreeNodeFlags_Selected;
      }
      ImGui::TreeNodeEx(label.c_str(), flags);
      if (ImGui::IsItemClicked()) {
         if (editorContext) {
            editorContext->SelectParticleSystem(particleSystem);
         }
      }
      ImGui::PopID();
   }
      ImGui::TreePop();
   }

   ImGui::End();
}

void RendererEditorController::ShowInspectorWindow() {
   ImGui::Begin("Inspector");

   auto* editorContext = GetActiveEditorContext();
   Object* selectedObject = editorContext ? editorContext->GetSelectedObject() : nullptr;
   ParticleSystem* selectedParticleSystem = editorContext ? editorContext->GetSelectedParticleSystem() : nullptr;

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

   if (!selectedObject && !selectedParticleSystem) {
      ImGui::Text("No selection");
      ImGui::End();
      return;
   }

   if (selectedParticleSystem) {
      std::string particleSystemName = selectedParticleSystem->GetName();
      char particleSystemNameBuffer[256]{};
      {
         const size_t copySize = std::min(particleSystemName.size(), sizeof(particleSystemNameBuffer) - 1);
         std::memcpy(particleSystemNameBuffer, particleSystemName.c_str(), copySize);
      }
      if (ImGui::InputText("Name", particleSystemNameBuffer, sizeof(particleSystemNameBuffer))) {
         selectedParticleSystem->SetName(particleSystemNameBuffer);
      }
      if (editorContext && editorContext->CanDeleteParticleSystem(selectedParticleSystem)) {
         if (ImGui::Button("Delete")) {
            editorContext->DeleteParticleSystem(selectedParticleSystem);
            ImGui::End();
            return;
         }
      }
      if (editorContext) {
         DrawParticleAssetDropTarget(*editorContext, selectedParticleSystem);
      }
      ImGui::Spacing();

      ParticleSystemEdit::Edit(selectedParticleSystem);
      if (editorContext && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsAnyItemActive()) {
         editorContext->MarkDirty();
      }
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
      ImGui::SameLine();
      if (ImGui::Button("Duplicate")) {
         editorContext->DuplicateSelectedObject();
      }
      ImGui::Spacing();
   }

   selectedObject->DrawComponentInspector();
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
            if (editorContext) {
               editorContext->AddComponentToSelectedObject(addableComponentTypeNames[editorSelectedAddComponentIndex_]);
            } else {
               selectedObject->AddComponentByTypeName(addableComponentTypeNames[editorSelectedAddComponentIndex_]);
            }
         }
      }
   }
   ImGui::PopID();

   ImGui::Spacing();
   if (editorContext) {
      DrawSelectedObjectAssetDropTargets(*editorContext, selectedObject);
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

   if (editorContext && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsAnyItemActive()) {
      editorContext->MarkDirty();
   }

   ImGui::End();
}

void RendererEditorController::ShowSceneOverlay(float viewportX, float viewportY, float viewportWidth, float viewportHeight) {
   auto* editorContext = GetActiveEditorContext();
   if (!editorContext) {
      return;
   }

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

void RendererEditorController::DrawAssetEntry(EditorSceneContext& editorContext, const EditorAssetEntry& entry) {
   ImGui::PushID(entry.assetId.c_str());

   const char* typeLabel = EditorAssetRegistry::GetAssetTypeLabel(entry.type);
   bool activated = false;
   constexpr float kAssetIconSize = 64.0f;
   constexpr float kAssetCellWidth = 92.0f;

   auto drawTextureIcon = [](Texture* texture, const ImVec2& size) {
      if (!texture) {
         ImGui::Button("[File]", size);
         return;
      }

      if (texture->GetMetadata().IsCubemap()) {
         ImGui::Button("[Cube]", size);
         return;
      }

      ImU64 texId{};
      const UINT64 gpuPtr = texture->GetTextureSrvHandleGPU().ptr;
      std::memcpy(&texId, &gpuPtr, sizeof(texId));
      ImGui::Image(ImTextureRef(texId), size);
   };

   auto getGenericAssetIcon = []() -> Texture* {
      if (Texture* icon = EngineContext::GetTexture("textures/editor/ic_system_folder_01_128.png")) {
         return icon;
      }
      if (Texture* icon = EngineContext::GetTexture("ic_system_folder_01_128")) {
         return icon;
      }
      return EngineContext::GetTexture("white1x1");
   };

   if (editorAssetIconView_) {
      ImGui::BeginGroup();
      const float cellStartX = ImGui::GetCursorPosX();
      const float iconOffsetX = std::max(0.0f, (kAssetCellWidth - kAssetIconSize) * 0.5f);
      ImGui::SetCursorPosX(cellStartX + iconOffsetX);
      if (entry.type == EditorAssetType::Texture && EnsureTextureLoaded(entry.assetId)) {
         drawTextureIcon(EngineContext::GetTexture(entry.assetId), ImVec2(kAssetIconSize, kAssetIconSize));
      } else {
         drawTextureIcon(getGenericAssetIcon(), ImVec2(kAssetIconSize, kAssetIconSize));
      }

      const std::string displayName = TruncateTextWithEllipsis(entry.displayName, kAssetCellWidth);
      const float textWidth = ImGui::CalcTextSize(displayName.c_str()).x;
      ImGui::SetCursorPosX(cellStartX + std::max(0.0f, (kAssetCellWidth - textWidth) * 0.5f));
      ImGui::TextUnformatted(displayName.c_str());
      ImGui::SetCursorPosX(cellStartX);
      ImGui::Dummy(ImVec2(kAssetCellWidth, 0.0f));
      ImGui::EndGroup();
      activated = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
   } else {
      std::string label = std::string("[") + typeLabel + "] " + entry.displayName;
      activated = ImGui::Selectable(label.c_str(), false) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
   }

   if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s\n%s", entry.assetId.c_str(), typeLabel);
   }

   EmitAssetDragPayload(entry);

   if (activated) {
      switch (entry.type) {
         case EditorAssetType::Model:
            editorContext.CreateModelFromAsset(entry.assetId);
            break;
         case EditorAssetType::Texture: {
            Texture* texture = EngineContext::GetTexture(entry.assetId);
            if (texture && !texture->GetMetadata().IsCubemap()) {
               editorContext.CreateSpriteFromTexture(entry.assetId);
            }
            break;
         }
         case EditorAssetType::Particle:
            editorContext.CreateParticleSystemFromAsset(entry.assetId);
            break;
         default:
            break;
      }
   }

   ImGui::PopID();
}

void RendererEditorController::DrawAssetTree(EditorSceneContext& editorContext) {
   const auto& assets = editorContext.GetAssetRegistry().GetAllAssets();
   std::unordered_map<std::string, std::vector<const EditorAssetEntry*>> childrenByParent;
   childrenByParent.reserve(assets.size());
   constexpr float kAssetCellWidth = 92.0f;

   for (const auto& entry : assets) {
      std::filesystem::path parentPath = std::filesystem::path(entry.assetId).parent_path();
      childrenByParent[parentPath.generic_string()].push_back(&entry);
   }

   auto sortChildren = [](std::vector<const EditorAssetEntry*>& children) {
      std::sort(children.begin(), children.end(),
         [](const EditorAssetEntry* lhs, const EditorAssetEntry* rhs) {
            if (lhs->type != rhs->type) {
               if (lhs->type == EditorAssetType::Folder) {
                  return true;
               }
               if (rhs->type == EditorAssetType::Folder) {
                  return false;
               }
            }
            return lhs->displayName < rhs->displayName;
         });
   };

   for (auto& [parent, children] : childrenByParent) {
      (void)parent;
      sortChildren(children);
   }

   std::function<void(const std::string&)> drawChildren = [&](const std::string& parent) {
      auto it = childrenByParent.find(parent);
      if (it == childrenByParent.end()) {
         return;
      }

      bool hasIconOnCurrentLine = false;
      for (const EditorAssetEntry* entry : it->second) {
         if (!entry) {
            continue;
         }

         if (entry->type == EditorAssetType::Folder) {
            hasIconOnCurrentLine = false;
            ImGui::PushID(entry->assetId.c_str());
            const bool open = ImGui::TreeNodeEx(entry->displayName.c_str(), ImGuiTreeNodeFlags_OpenOnArrow);
            EmitAssetDragPayload(*entry);
            if (open) {
               drawChildren(entry->assetId);
               ImGui::TreePop();
            }
            ImGui::PopID();
         } else {
            DrawAssetEntry(editorContext, *entry);
            if (editorAssetIconView_) {
               const float nextItemRight = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + kAssetCellWidth;
               const float contentRight = ImGui::GetWindowPos().x + ImGui::GetContentRegionMax().x;
               if (nextItemRight < contentRight) {
                  ImGui::SameLine();
                  hasIconOnCurrentLine = true;
               } else {
                  hasIconOnCurrentLine = false;
               }
            }
         }
      }

      if (hasIconOnCurrentLine) {
         ImGui::NewLine();
      }
   };

   drawChildren("");
}

void RendererEditorController::EmitAssetDragPayload(const EditorAssetEntry& entry) const {
   const char* payloadName = nullptr;
   switch (entry.type) {
      case EditorAssetType::Model:
         payloadName = "EDITOR_ASSET_MODEL";
         break;
      case EditorAssetType::Texture:
         payloadName = "EDITOR_ASSET_TEXTURE";
         break;
      case EditorAssetType::Particle:
         payloadName = "EDITOR_ASSET_PARTICLE";
         break;
      case EditorAssetType::Prefab:
         payloadName = "EDITOR_ASSET_PREFAB";
         break;
      case EditorAssetType::Scene:
         payloadName = "EDITOR_ASSET_SCENE";
         break;
      default:
         break;
   }

   if (!payloadName) {
      return;
   }

   if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
      ImGui::SetDragDropPayload(payloadName, entry.assetId.c_str(), entry.assetId.size() + 1);
      ImGui::Text("%s", entry.assetId.c_str());
      ImGui::EndDragDropSource();
   }
}

bool RendererEditorController::EnsureTextureLoaded(const std::string& textureAssetId) {
   if (textureAssetId.empty()) {
      return false;
   }

   if (editorLoadedTextureAssets_.contains(textureAssetId)) {
      return true;
   }

   if (EngineContext::GetTexture(textureAssetId)) {
      editorLoadedTextureAssets_.insert(textureAssetId);
      return true;
   }

   const std::filesystem::path texturePath(textureAssetId);
   if (EngineContext::GetTexture(texturePath.stem().string()) || EngineContext::GetTexture(texturePath.filename().string())) {
      editorLoadedTextureAssets_.insert(textureAssetId);
      return true;
   }
   return false;
}

void RendererEditorController::DrawSelectedObjectAssetDropTargets(EditorSceneContext& editorContext, Object* selectedObject) {
   if (!selectedObject) {
      return;
   }

   if (auto* modelAssetComponent = selectedObject->GetComponent<ModelAssetComponent>()) {
      ImGui::SeparatorText("Model Asset Drop");
      const std::string currentAsset = modelAssetComponent->GetAssetId().empty() ? "<none>" : modelAssetComponent->GetAssetId();
      ImGui::Button(currentAsset.c_str(), ImVec2(-1.0f, 0.0f));
      if (ImGui::BeginDragDropTarget()) {
         if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_ASSET_MODEL")) {
            const char* assetId = static_cast<const char*>(payload->Data);
            if (assetId && payload->DataSize > 1) {
               editorContext.SetModelAsset(selectedObject, assetId);
            }
         }
         ImGui::EndDragDropTarget();
      }
   }

   if (auto* materialComponent = selectedObject->GetComponent<MaterialComponent>()) {
      ImGui::SeparatorText("Texture Asset Drop");
      const std::string currentTexture = materialComponent->GetTextureName(0).empty() ? "<none>" : materialComponent->GetTextureName(0);
      ImGui::Button(("Slot 0: " + currentTexture).c_str(), ImVec2(-1.0f, 0.0f));
      if (ImGui::BeginDragDropTarget()) {
         if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_ASSET_TEXTURE")) {
            const char* assetId = static_cast<const char*>(payload->Data);
            if (assetId && payload->DataSize > 1) {
               EnsureTextureLoaded(assetId);
               editorContext.SetMaterialTexture(selectedObject, 0, assetId);
            }
         }
         ImGui::EndDragDropTarget();
      }
   }
}

void RendererEditorController::DrawParticleAssetDropTarget(EditorSceneContext& editorContext, ParticleSystem* particleSystem) {
   if (!particleSystem) {
      return;
   }

   ImGui::SeparatorText("Particle Asset Drop");
   ImGui::Button("Drop particle json or texture here", ImVec2(-1.0f, 0.0f));
   if (!ImGui::BeginDragDropTarget()) {
      return;
   }

   if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_ASSET_PARTICLE")) {
      const char* assetId = static_cast<const char*>(payload->Data);
      if (assetId && payload->DataSize > 1) {
         particleSystem->LoadFromJson((std::filesystem::path("resources") / assetId).generic_string());
         editorContext.MarkDirty();
      }
   }

   if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_ASSET_TEXTURE")) {
      const char* assetId = static_cast<const char*>(payload->Data);
      if (assetId && payload->DataSize > 1) {
         EnsureTextureLoaded(assetId);
         particleSystem->SetTextureName(assetId);
         editorContext.MarkDirty();
      }
   }

   ImGui::EndDragDropTarget();
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

} // namespace GameEngine

#endif
