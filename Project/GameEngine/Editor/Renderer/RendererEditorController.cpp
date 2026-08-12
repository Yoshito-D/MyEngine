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
#include "Component/IObjectComponent.h"
#include "Model/Model.h"
#include "Sprite/Sprite.h"
#include "Text/UIText.h"
#include "Object/Skybox/Skybox.h"
#include "Effect/ParticleSystem.h"
#include "Editor/Particle/ParticleSystemEditor.h"
#include "Editor/EditorAssetRegistry.h"
#include "Editor/EditorSceneContext.h"
#include "Framework/EngineContext.h"
#include "Graphics/Texture.h"
#include "Utility/ImGuiHelper.h"
#include "Scene/BaseScene.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <functional>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <unordered_set>

namespace GameEngine {

namespace {
constexpr int kJsonIndentSize = 3;
constexpr unsigned char kUtf8ContinuationByteMask = 0xC0;
constexpr unsigned char kUtf8ContinuationByteTag = 0x80;
constexpr unsigned char kAsciiControlCharacterLimit = 0x20;
constexpr float kHierarchyDropGuideThickness = 2.0f;
constexpr int kEmptyCStringPayloadSize = 1;
constexpr size_t kInspectorNameBufferSize = 256;
constexpr int kFirstUniqueNameSuffix = 1;

const char* Tr(const char* japanese, const char* english) {
   return ImGuiHelper::Localize({ japanese, english });
}

std::string StableWindowLabel(const char* visibleLabel, const char* stableId) {
   // 表示言語が変わってもImGuiのウィンドウ状態を保持するため、###以降に固定IDを置く。
   return std::string(visibleLabel) + "###" + stableId;
}

void PopLastUtf8Codepoint(std::string& text) {
   if (text.empty()) {
      return;
   }

   size_t erasePos = text.size() - 1;
   // UTF-8継続バイトをさかのぼり、切り詰め時に不正な文字列を作らない。
   while (erasePos > 0) {
      const unsigned char c = static_cast<unsigned char>(text[erasePos]);
      if ((c & kUtf8ContinuationByteMask) != kUtf8ContinuationByteTag) {
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

constexpr const char* kSceneCatalogPath = "resources/game/scene_catalog.json";

bool LoadSceneCatalogData(nlohmann::json& catalogData, std::string& errorMessage) {
   std::ifstream file(kSceneCatalogPath);
   if (!file.is_open()) {
      errorMessage = "Scene catalog could not be opened";
      return false;
   }

   try {
      file >> catalogData;
   } catch (const nlohmann::json::exception& exception) {
      errorMessage = "Scene catalog contains invalid JSON: " + std::string(exception.what());
      return false;
   }

   if (!catalogData.is_object() ||
      !catalogData.contains("scenes") ||
      !catalogData.at("scenes").is_object()) {
      errorMessage = "Scene catalog must contain a scenes object";
      return false;
   }
   return true;
}

bool SaveJsonFile(
   const std::filesystem::path& filePath,
   const nlohmann::json& jsonData,
   std::string& errorMessage) {
   std::error_code error;
   std::filesystem::create_directories(filePath.parent_path(), error);
   if (error) {
      errorMessage = "Could not create directory: " + filePath.parent_path().generic_string();
      return false;
   }

   std::ofstream file(filePath);
   if (!file.is_open()) {
      errorMessage = "Could not write: " + filePath.generic_string();
      return false;
   }
   file << jsonData.dump(kJsonIndentSize);
   file.flush();
   if (!file.good()) {
      errorMessage = "Write failed: " + filePath.generic_string();
      return false;
   }
   return true;
}

bool IsValidSceneName(const std::string& sceneName) {
   if (sceneName.empty() || sceneName == "." || sceneName == ".." ||
      sceneName.back() == '.' ||
      std::isspace(static_cast<unsigned char>(sceneName.front())) ||
      std::isspace(static_cast<unsigned char>(sceneName.back()))) {
      return false;
   }

   constexpr const char* kInvalidFileNameCharacters = "\\/:*?\"<>|";
   return sceneName.find_first_of(kInvalidFileNameCharacters) == std::string::npos &&
      std::none_of(sceneName.begin(), sceneName.end(),
         [](unsigned char character) {
            return character < kAsciiControlCharacterLimit;
         });
}

void DrawHierarchyInsertionDropTarget(
   EditorSceneContext& editorContext,
   Object* targetObject,
   EditorSceneContext::HierarchyDropPosition dropPosition) {
   if (!targetObject) {
      return;
   }

   // 通常のItemSpacingを実際にドロップできる領域へ置き換え、行間を大きく広げずに
   // オブジェクト同士の境界を狙えるようにする。
   const ImGuiStyle& style = ImGui::GetStyle();
   constexpr float kMinimumDropTargetHeight = 6.0f;
   const float dropTargetHeight = std::max(style.ItemSpacing.y, kMinimumDropTargetHeight);
   ImGui::SetCursorPosY(ImGui::GetCursorPosY() - style.ItemSpacing.y);
   ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 0.0f));
   ImGui::PushID(targetObject);
   ImGui::PushID(static_cast<int>(dropPosition));
   ImGui::InvisibleButton(
      "##HierarchyInsertionDropTarget",
      ImVec2(std::max(ImGui::GetContentRegionAvail().x, 1.0f), dropTargetHeight));

   if (ImGui::BeginDragDropTarget()) {
      constexpr ImGuiDragDropFlags acceptFlags =
         ImGuiDragDropFlags_AcceptBeforeDelivery |
         ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
      if (const ImGuiPayload* payload =
         ImGui::AcceptDragDropPayload("EDITOR_SCENE_OBJECT", acceptFlags)) {
         if (payload->IsPreview()) {
            const ImVec2 targetMin = ImGui::GetItemRectMin();
            const ImVec2 targetMax = ImGui::GetItemRectMax();
            const float guideY = (targetMin.y + targetMax.y) * 0.5f;
            ImGui::GetWindowDrawList()->AddLine(
               ImVec2(targetMin.x, guideY),
               ImVec2(targetMax.x, guideY),
               ImGui::GetColorU32(ImGuiCol_DragDropTarget),
               kHierarchyDropGuideThickness);
         }
         if (payload->IsDelivery() && payload->Data && payload->DataSize > kEmptyCStringPayloadSize) {
            const char* draggedId = static_cast<const char*>(payload->Data);
            if (Object* draggedObject = Object::FindByEntityId(draggedId)) {
               editorContext.ReorderObject(draggedObject, targetObject, dropPosition);
            }
         }
      }
      ImGui::EndDragDropTarget();
   }

   ImGui::PopID();
   ImGui::PopID();
   ImGui::PopStyleVar();
}
} // namespace

void RendererEditorController::Initialize(AssetManager* assetManager) {
   assetManager_ = assetManager;
   RefreshSceneCatalog();
}

void RendererEditorController::BeginEditorFrame() {
   auto* editorContext = GetActiveEditorContext();
   if (!editorContext) {
      return;
   }

   // 前フレームのUIが参照し終えた後で、遅延削除されたオブジェクトを安全に破棄する。
   editorContext->GetObjectStore().FlushDeferredDeletes();
   editorContext->HandleEditorShortcuts();
}

void RendererEditorController::ShowPlayModeToolbar() {
   const std::string windowLabel = StableWindowLabel(Tr("再生", "Play Mode"), "PlayModeToolbar");
   ImGui::Begin(windowLabel.c_str());

   const PlayMode mode = EngineContext::GetPlayMode();
   const bool isEdit = mode == PlayMode::Edit;
   const bool isPlaying = mode == PlayMode::Playing;
   const bool isPaused = mode == PlayMode::Paused;

   ImGui::BeginDisabled(isPlaying);
   if (ImGui::Button(isPaused ? "Resume" : "Play")) {
      EngineContext::RequestPlayModeStart();
   }
   ImGui::EndDisabled();

   ImGui::SameLine();
   ImGui::BeginDisabled(isEdit);
   if (ImGui::Button("Stop")) {
      EngineContext::RequestPlayModeStop();
   }
   ImGui::EndDisabled();

   ImGui::SameLine();
   ImGui::BeginDisabled(!isPlaying);
   if (ImGui::Button("Pause")) {
      EngineContext::RequestPlayModePause();
   }
   ImGui::EndDisabled();

   ImGui::SameLine();
   ImGui::BeginDisabled(!isPaused);
   if (ImGui::Button("Step")) {
      EngineContext::RequestPlayModeStep();
   }
   ImGui::EndDisabled();

   float timeScale = EngineContext::GetTimeScale();
   if (ImGui::SliderFloat("Time Scale", &timeScale, 0.0f, 2.0f, "%.2f")) {
      EngineContext::SetTimeScale(timeScale);
   }

   ImGui::Text("Mode: %s", EngineContext::GetPlayModeName());
   ImGui::Text("Delta Time: %.4f", EngineContext::GetDeltaTime());
   ImGui::Text("Unscaled Delta Time: %.4f", EngineContext::GetUnscaledDeltaTime());

   ImGui::End();
}

void RendererEditorController::ShowAssetWindow() {
   const std::string windowLabel = StableWindowLabel(Tr("アセット", "Assets"), "Assets");
   ImGui::Begin(windowLabel.c_str());

   auto* editorContext = GetActiveEditorContext();
   if (editorContext) {
      const std::string dirtyMark = editorContext->IsDirty() ? " *" : "";
      ImGui::Text("%s%s", Tr("シーン", "Scene"), dirtyMark.c_str());
      ImGui::Separator();
      const bool canUseSceneFileButtons = !EngineContext::IsInPlayMode();
      ImGui::BeginDisabled(!canUseSceneFileButtons);
      if (ImGui::Button(Tr("シーンを保存", "Save Scene"))) {
         editorContext->Save();
      }
      ImGui::SameLine();
      if (ImGui::Button(Tr("シーンを再読み込み", "Reload Scene"))) {
         editorContext->Load();
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::Button(Tr("アセット再スキャン", "Rescan Assets"))) {
         editorContext->GetAssetRegistry().Scan();
      }
      ImGui::TextDisabled("%s", editorContext->GetSceneFilePath().generic_string().c_str());
      if (!editorContext->GetLastStatusMessage().empty()) {
         ImGui::TextWrapped("%s", editorContext->GetLastStatusMessage().c_str());
      }
      ImGui::Spacing();

      ImGui::Text("%s", Tr("シーン管理", "Scene Management"));
      ImGui::Separator();
      const BaseScene* activeScene = BaseScene::GetCurrentScene();
      const std::string activeSceneName = activeScene ? activeScene->GetEditorSceneName() : std::string{};
      ImGui::Text("%s: %s", Tr("現在", "Current"), activeSceneName.c_str());

      const char* selectedSceneLabel = editorSelectedSceneName_.empty()
         ? Tr("シーンを選択", "Select a scene")
         : editorSelectedSceneName_.c_str();
      if (ImGui::BeginCombo(Tr("シーン一覧", "Scenes"), selectedSceneLabel)) {
         for (const std::string& sceneName : editorSceneNames_) {
            const bool isSelected = sceneName == editorSelectedSceneName_;
            if (ImGui::Selectable(sceneName.c_str(), isSelected)) {
               editorSelectedSceneName_ = sceneName;
            }
            if (isSelected) {
               ImGui::SetItemDefaultFocus();
            }
         }
         ImGui::EndCombo();
      }

      const bool canOpenScene =
         canUseSceneFileButtons &&
         !editorContext->IsDirty() &&
         !editorSelectedSceneName_.empty() &&
         editorSelectedSceneName_ != activeSceneName;
      ImGui::BeginDisabled(!canOpenScene);
      if (ImGui::Button(Tr("選択シーンを開く", "Open Selected Scene"))) {
         EngineContext::ChangeScene(editorSelectedSceneName_);
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::Button(Tr("一覧を更新", "Refresh List"))) {
         RefreshSceneCatalog();
      }
      if (editorContext->IsDirty()) {
         ImGui::TextDisabled("%s", Tr(
            "別のシーンを開く前に現在のシーンを保存してください",
            "Save the current scene before opening another scene"));
      }

      ImGui::InputText(Tr("新しいシーン名", "New Scene Name"), editorNewSceneName_, sizeof(editorNewSceneName_));
      ImGui::BeginDisabled(!canUseSceneFileButtons);
      if (ImGui::Button(Tr("シーンを作成", "Create Scene"))) {
         CreateEditorScene(editorNewSceneName_);
      }
      ImGui::EndDisabled();

      const char* releaseStartLabel = editorReleaseStartSceneName_.empty()
         ? Tr("未設定", "Not set")
         : editorReleaseStartSceneName_.c_str();
      if (ImGui::BeginCombo(Tr("リリース開始シーン", "Release Start Scene"), releaseStartLabel)) {
         for (const std::string& sceneName : editorSceneNames_) {
            const bool isSelected = sceneName == editorReleaseStartSceneName_;
            if (ImGui::Selectable(sceneName.c_str(), isSelected)) {
               SetReleaseStartScene(sceneName);
            }
            if (isSelected) {
               ImGui::SetItemDefaultFocus();
            }
         }
         ImGui::EndCombo();
      }
      ImGui::TextDisabled("%s", Tr(
         "次回の起動時とリリースビルドはこのシーンから開始します",
         "The next launch and release build start from this scene"));
      if (!editorSceneCatalogStatus_.empty()) {
         ImGui::TextWrapped("%s", editorSceneCatalogStatus_.c_str());
      }
      ImGui::Spacing();

      ImGui::Text("%s", Tr("プロジェクト", "Project"));
      ImGui::Separator();
      ImGui::Checkbox(Tr("アイコン表示", "Icon View"), &editorAssetIconView_);
      if (editorContext->GetAssetRegistry().GetAllAssets().empty()) {
         ImGui::Text("%s", Tr("resources 以下にアセットが見つかりません", "No assets found under resources"));
      } else {
         DrawAssetTree(*editorContext);
      }
      ImGui::Spacing();
   } else {
      ImGui::Text("%s", Tr("エディタシーンコンテキストを利用できません", "Editor scene context is not available"));
      ImGui::Spacing();
   }

   ImGui::End();
}

void RendererEditorController::ShowHierarchyWindow() {
   const std::string windowLabel = StableWindowLabel(Tr("ヒエラルキー", "Hierarchy"), "Hierarchy");
   ImGui::Begin(windowLabel.c_str());

   auto* editorContext = GetActiveEditorContext();
   const auto sceneObjects = editorContext ? editorContext->CollectEditableObjects() : CollectSceneObjects();
   const auto particleSystems = editorContext ? editorContext->CollectEditableParticleSystems() : ParticleSystem::GetRegisteredParticleSystems();

   if (editorContext && ImGui::BeginPopupContextWindow("HierarchyCreateContext", ImGuiPopupFlags_MouseButtonRight)) {
      if (ImGui::MenuItem(Tr("選択を複製", "Duplicate Selected"), "Ctrl+D")) {
         editorContext->DuplicateSelectedObject();
      }
      if (ImGui::MenuItem(Tr("選択を削除", "Delete Selected"), "Delete")) {
         editorContext->DeleteSelection();
      }
      if (Object* selected = editorContext->GetSelectedObject();
         selected && !selected->GetParentEntityId().empty() &&
         ImGui::MenuItem(Tr("親子関係を解除", "Unparent Selected"))) {
         editorContext->ReorderObject(
            selected,
            nullptr,
            EditorSceneContext::HierarchyDropPosition::After);
      }
      ImGui::Separator();

      if (ImGui::MenuItem(Tr("空のオブジェクト", "Empty Object"))) {
         editorContext->CreateEmptyObject();
      }

      if (ImGui::BeginMenu(Tr("モデル", "Model"))) {
         const auto& modelAssets = editorContext->GetAssetRegistry().GetModelAssets();
         if (modelAssets.empty()) {
            ImGui::TextDisabled("%s", Tr(".obj / .gltf モデルがありません", "No .obj or .gltf models"));
         } else {
            for (const auto& entry : modelAssets) {
               if (ImGui::MenuItem(entry.displayName.c_str())) {
                  editorContext->CreateModelFromAsset(entry.assetId);
               }
            }
         }
         ImGui::EndMenu();
      }

      if (ImGui::BeginMenu(Tr("スプライト", "Sprite"))) {
         const auto& textureAssets = editorContext->GetAssetRegistry().GetTextureAssets();
         if (textureAssets.empty()) {
            ImGui::TextDisabled("%s", Tr("テクスチャファイルがありません", "No texture files"));
         } else {
            for (const auto& entry : textureAssets) {
               if (ImGui::MenuItem(entry.displayName.c_str())) {
                  editorContext->CreateSpriteFromTexture(entry.assetId);
               }
            }
         }
         ImGui::EndMenu();
      }

      if (ImGui::MenuItem(Tr("UIテキスト", "UI Text"))) {
         editorContext->CreateUIText();
      }

      if (ImGui::BeginMenu(Tr("ライト", "Light"))) {
         if (ImGui::MenuItem(Tr("ディレクショナル", "Directional"))) {
            editorContext->CreateDirectionalLight();
         }
         if (ImGui::MenuItem(Tr("ポイント", "Point"))) {
            editorContext->CreatePointLight();
         }
         if (ImGui::MenuItem(Tr("スポット", "Spot"))) {
            editorContext->CreateSpotLight();
         }
         if (ImGui::MenuItem(Tr("エリア", "Area"))) {
            editorContext->CreateAreaLight();
         }
         ImGui::EndMenu();
      }

      if (ImGui::BeginMenu(Tr("パーティクルシステム", "Particle System"))) {
         const auto& particleAssets = editorContext->GetAssetRegistry().GetParticleAssets();
         if (particleAssets.empty()) {
            ImGui::TextDisabled("%s", Tr("パーティクル json がありません", "No particle json files"));
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
      ImGui::Text("%s", Tr("オブジェクトがありません", "No objects"));
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
      // シーン切り替えや外部削除で一覧から消えた選択ポインターをインスペクターへ渡さない。
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
   const std::string sceneObjectsLabel = std::string(Tr("シーンオブジェクト", "Scene Objects")) + "###HierarchySceneObjects";
   const bool sceneObjectsOpen = ImGui::TreeNodeEx(sceneObjectsLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
   if (editorContext && ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_SCENE_OBJECT")) {
         const char* draggedId = static_cast<const char*>(payload->Data);
         if (Object* draggedObject = draggedId ? Object::FindByEntityId(draggedId) : nullptr) {
            editorContext->ReorderObject(
               draggedObject,
               nullptr,
               EditorSceneContext::HierarchyDropPosition::After);
         }
      }
      ImGui::EndDragDropTarget();
   }
   if (sceneObjectsOpen) {
      std::unordered_set<Object*> renderedObjects;
      std::function<void(Object*)> drawEntityNode;
      std::function<void(const std::vector<Object*>&)> drawEntityList;
      drawEntityNode = [&](Object* object) {
         if (!object || renderedObjects.contains(object)) {
            return;
         }
         renderedObjects.insert(object);

         std::vector<Object*> children;
         for (Object* candidate : sceneObjects) {
            if (candidate && candidate->GetParentEntityId() == object->GetEntityId()) {
               children.push_back(candidate);
            }
         }

         ImGui::PushID(object);
         const std::string objectName = object->GetObjectName();
         ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
         if (children.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
         }
         if (selectedObject == object) {
            flags |= ImGuiTreeNodeFlags_Selected;
         }

         const bool isOpen = ImGui::TreeNodeEx(objectName.c_str(), flags);
         const bool clicked = ImGui::IsItemClicked();
         if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            const std::string& entityId = object->GetEntityId();
            ImGui::SetDragDropPayload("EDITOR_SCENE_OBJECT", entityId.c_str(), entityId.size() + 1);
            ImGui::Text("%s", objectName.c_str());
            ImGui::TextDisabled("%s", Tr(
               "行間: 並び替え  オブジェクト上: 子にする",
               "Between rows: reorder  On object: make child"));
            ImGui::EndDragDropSource();
         }
         if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_SCENE_OBJECT")) {
               const char* draggedId = static_cast<const char*>(payload->Data);
               Object* draggedObject = draggedId ? Object::FindByEntityId(draggedId) : nullptr;
               if (draggedObject && editorContext) {
                  editorContext->ReorderObject(
                     draggedObject,
                     object,
                     EditorSceneContext::HierarchyDropPosition::Into);
               }
            }
            ImGui::EndDragDropTarget();
         }
         if (clicked && editorContext) {
            editorContext->SelectObject(object);
         }

         if (isOpen && !children.empty()) {
            drawEntityList(children);
            ImGui::TreePop();
         }
         ImGui::PopID();
      };

      drawEntityList = [&](const std::vector<Object*>& objects) {
         Object* lastObject = nullptr;
         for (Object* object : objects) {
            if (!object || renderedObjects.contains(object)) {
               continue;
            }
            if (editorContext) {
               DrawHierarchyInsertionDropTarget(
                  *editorContext,
                  object,
                  EditorSceneContext::HierarchyDropPosition::Before);
            }
            drawEntityNode(object);
            lastObject = object;
         }
         if (editorContext && lastObject) {
            DrawHierarchyInsertionDropTarget(
               *editorContext,
               lastObject,
               EditorSceneContext::HierarchyDropPosition::After);
         }
      };

      std::vector<Object*> rootObjects;
      rootObjects.reserve(sceneObjects.size());
      for (Object* object : sceneObjects) {
         if (!object) {
            continue;
         }
         const std::string& parentId = object->GetParentEntityId();
         const bool hasVisibleParent = !parentId.empty() &&
            std::any_of(sceneObjects.begin(), sceneObjects.end(),
               [&parentId](const Object* candidate) {
                  return candidate && candidate->GetEntityId() == parentId;
               });
         if (!hasVisibleParent) {
            rootObjects.push_back(object);
         }
      }
      drawEntityList(rootObjects);

      // 循環や壊れた参照があってもEntityをヒエラルキーから消さない。
      std::vector<Object*> remainingObjects;
      remainingObjects.reserve(sceneObjects.size() - renderedObjects.size());
      for (Object* object : sceneObjects) {
         if (object && !renderedObjects.contains(object)) {
            remainingObjects.push_back(object);
         }
      }
      drawEntityList(remainingObjects);
      ImGui::TreePop();
   }

   ImGui::SetNextItemOpen(true, ImGuiCond_Once);
   const std::string particleSystemsLabel = std::string(Tr("パーティクルシステム", "Particle Systems")) + "###HierarchyParticleSystems";
   if (ImGui::TreeNodeEx(particleSystemsLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
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

void RendererEditorController::RefreshSceneCatalog() {
   nlohmann::json catalogData;
   std::string errorMessage;
   if (!LoadSceneCatalogData(catalogData, errorMessage)) {
      editorSceneNames_.clear();
      editorReleaseStartSceneName_.clear();
      editorSceneCatalogStatus_ = std::move(errorMessage);
      return;
   }

   editorSceneNames_.clear();
   for (const auto& [sceneName, scenePath] : catalogData.at("scenes").items()) {
      if (!sceneName.empty() && scenePath.is_string()) {
         editorSceneNames_.push_back(sceneName);
      }
   }
   std::sort(editorSceneNames_.begin(), editorSceneNames_.end());
   editorReleaseStartSceneName_ = catalogData.value("initialScene", "");

   if (editorSelectedSceneName_.empty() ||
      std::find(editorSceneNames_.begin(), editorSceneNames_.end(), editorSelectedSceneName_) ==
         editorSceneNames_.end()) {
      editorSelectedSceneName_ = editorReleaseStartSceneName_;
   }
   editorSceneCatalogStatus_.clear();
}

bool RendererEditorController::CreateEditorScene(const std::string& sceneName) {
   if (!IsValidSceneName(sceneName)) {
      editorSceneCatalogStatus_ = "Create failed: invalid scene name";
      return false;
   }

   nlohmann::json catalogData;
   std::string errorMessage;
   if (!LoadSceneCatalogData(catalogData, errorMessage)) {
      editorSceneCatalogStatus_ = std::move(errorMessage);
      return false;
   }

   const std::string foldedSceneName = [&sceneName]() {
      std::string folded = sceneName;
      std::transform(folded.begin(), folded.end(), folded.begin(),
         [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
         });
      return folded;
   }();
   for (const auto& registeredScene : catalogData.at("scenes").items()) {
      const std::string& registeredName = registeredScene.key();
      std::string foldedRegisteredName = registeredName;
      std::transform(foldedRegisteredName.begin(), foldedRegisteredName.end(), foldedRegisteredName.begin(),
         [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
         });
      if (foldedRegisteredName == foldedSceneName) {
         editorSceneCatalogStatus_ = "Create failed: scene already exists";
         return false;
      }
   }

   const std::filesystem::path sceneFilePath =
      std::filesystem::path("resources") / "game" / "scenes" / (sceneName + ".json");
   if (std::filesystem::exists(sceneFilePath)) {
      editorSceneCatalogStatus_ = "Create failed: scene file already exists";
      return false;
   }

   nlohmann::json sceneData = {
      { "version", EditorSceneContext::kCurrentSceneFormatVersion },
      { "sceneName", sceneName },
      { "objects", nlohmann::json::array() },
      { "sceneObjects", nlohmann::json::array() },
      { "sceneParticleSystems", nlohmann::json::array() },
      { "hierarchyOrder", nlohmann::json::array() },
      { "cameras", {
         { "brain", { { "defaultBlendTime", 0.0f } } },
         { "virtualCameras", nlohmann::json::array() }
      } },
      { "environment", nlohmann::json::object() }
   };
   if (!SaveJsonFile(sceneFilePath, sceneData, errorMessage)) {
      editorSceneCatalogStatus_ = std::move(errorMessage);
      return false;
   }

   catalogData["scenes"][sceneName] = "game/scenes/" + sceneName + ".json";
   if (!SaveJsonFile(kSceneCatalogPath, catalogData, errorMessage)) {
      std::error_code rollbackError;
      std::filesystem::remove(sceneFilePath, rollbackError);
      editorSceneCatalogStatus_ = rollbackError
         ? "Scene file was created, but catalog update failed: " + errorMessage
         : "Catalog update failed; the new scene file was rolled back: " + errorMessage;
      return false;
   }

   RefreshSceneCatalog();
   editorSelectedSceneName_ = sceneName;
   editorSceneCatalogStatus_ = "Created scene: " + sceneName;
   return true;
}

bool RendererEditorController::SetReleaseStartScene(const std::string& sceneName) {
   nlohmann::json catalogData;
   std::string errorMessage;
   if (!LoadSceneCatalogData(catalogData, errorMessage)) {
      editorSceneCatalogStatus_ = std::move(errorMessage);
      return false;
   }
   if (sceneName.empty() || !catalogData.at("scenes").contains(sceneName)) {
      editorSceneCatalogStatus_ = "Release start scene is not registered";
      return false;
   }

   catalogData["initialScene"] = sceneName;
   if (!SaveJsonFile(kSceneCatalogPath, catalogData, errorMessage)) {
      editorSceneCatalogStatus_ = std::move(errorMessage);
      return false;
   }

   editorReleaseStartSceneName_ = sceneName;
   editorSceneCatalogStatus_ = "Release start scene: " + sceneName;
   return true;
}

void RendererEditorController::ShowInspectorWindow() {
   const std::string windowLabel = StableWindowLabel(Tr("インスペクター", "Inspector"), "Inspector");
   ImGui::Begin(windowLabel.c_str());

   auto* editorContext = GetActiveEditorContext();
   Object* selectedObject = editorContext ? editorContext->GetSelectedObject() : nullptr;
   ParticleSystem* selectedParticleSystem = editorContext ? editorContext->GetSelectedParticleSystem() : nullptr;

   if (!selectedObject && !selectedParticleSystem) {
      editorComponentSaveStatusObject_ = nullptr;
      editorComponentSaveStatus_.clear();
      ImGui::Text("%s", Tr("未選択", "No selection"));
      ImGui::End();
      return;
   }

   if (selectedParticleSystem) {
      editorComponentSaveStatusObject_ = nullptr;
      editorComponentSaveStatus_.clear();
      std::string particleSystemName = selectedParticleSystem->GetName();
      char particleSystemNameBuffer[kInspectorNameBufferSize]{};
      {
         const size_t copySize = std::min(particleSystemName.size(), sizeof(particleSystemNameBuffer) - 1);
         std::memcpy(particleSystemNameBuffer, particleSystemName.c_str(), copySize);
      }
      if (ImGui::InputText(Tr("名前", "Name"), particleSystemNameBuffer, sizeof(particleSystemNameBuffer))) {
         selectedParticleSystem->SetName(particleSystemNameBuffer);
      }
      if (editorContext && editorContext->CanDeleteParticleSystem(selectedParticleSystem)) {
         if (ImGui::Button(Tr("削除", "Delete"))) {
            editorContext->DeleteParticleSystem(selectedParticleSystem);
            ImGui::End();
            return;
         }
      }
      if (editorContext) {
         DrawParticleAssetDropTarget(*editorContext, selectedParticleSystem);
         editorContext->DrawGizmoInspectorControls();
      }
      ImGui::Spacing();

      ParticleSystemEditor::Edit(selectedParticleSystem);
      if (editorContext && EngineContext::IsPlayModeEdit() &&
         ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsAnyItemActive()) {
         editorContext->MarkDirty();
      }
      ImGui::End();
      return;
   }

   if (editorComponentSaveStatusObject_ != selectedObject) {
      editorComponentSaveStatusObject_ = selectedObject;
      editorComponentSaveStatus_.clear();
   }

   std::string objectName = selectedObject->GetObjectName();
   char objectNameBuffer[kInspectorNameBufferSize]{};
   {
      const size_t copySize = std::min(objectName.size(), sizeof(objectNameBuffer) - 1);
      std::memcpy(objectNameBuffer, objectName.c_str(), copySize);
   }
   if (ImGui::InputText(Tr("名前", "Name"), objectNameBuffer, sizeof(objectNameBuffer))) {
      selectedObject->SetObjectName(objectNameBuffer);
   }
   ImGui::Spacing();

   if (editorContext) {
      const bool editorOwned = editorContext->IsEditorOwned(selectedObject);
      ImGui::Text("%s: %s", Tr("所有者", "Owner"), editorOwned ? Tr("エディタ", "Editor") : Tr("シーン", "Scene"));
      ImGui::Text("%s: %s", Tr("Entity ID", "Entity ID"), selectedObject->GetEntityId().c_str());
      if (!selectedObject->GetParentEntityId().empty()) {
         ImGui::Text("%s: %s", Tr("親Entity", "Parent Entity"), selectedObject->GetParentEntityId().c_str());
      }
      if (editorContext->CanDeleteObject(selectedObject)) {
         if (ImGui::Button(Tr("削除", "Delete"))) {
            editorContext->DeleteSelectedObject();
            ImGui::End();
            return;
         }
      }
      ImGui::SameLine();
      if (ImGui::Button(Tr("複製", "Duplicate"))) {
         editorContext->DuplicateSelectedObject();
      }
      ImGui::Spacing();
   }

   const ComponentInspectorAction componentAction =
      selectedObject->DrawComponentInspector(EngineContext::IsInPlayMode());

   if (!componentAction.savedTypeName.empty()) {
      const std::string componentDisplayName =
         LocalizeObjectComponentTypeName(componentAction.savedTypeName.c_str());
      if (EngineContext::SavePlayModeComponent(*selectedObject, componentAction.savedTypeName)) {
         editorComponentSaveStatus_ =
            std::string(Tr("保存しました: ", "Saved: ")) + componentDisplayName;
      } else {
         editorComponentSaveStatus_ =
            std::string(Tr("保存できませんでした: ", "Could not save: ")) + componentDisplayName;
      }
   }

   if (!editorComponentSaveStatus_.empty()) {
      ImGui::TextWrapped("%s", editorComponentSaveStatus_.c_str());
      ImGui::Spacing();
   }

   if (!componentAction.removedTypeName.empty()) {
      if (editorContext) {
         editorContext->RemoveComponentFromSelectedObject(componentAction.removedTypeName);
      } else {
         selectedObject->RemoveComponentByTypeName(componentAction.removedTypeName);
      }
   }

   ImGui::PushID("InspectorAddComponent");
   const std::string addComponentHeader = std::string(Tr("コンポーネント追加", "Add Component")) + "###InspectorAddComponentHeader";
   if (ImGui::CollapsingHeader(addComponentHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
      std::vector<std::string> addableComponentTypeNames;
      const auto registeredTypeNames = ComponentRegistry::GetInstance().GetRegisteredTypeNames(*selectedObject);
      addableComponentTypeNames.reserve(registeredTypeNames.size());
      for (const auto& typeName : registeredTypeNames) {
         if (!selectedObject->HasComponentByTypeName(typeName)) {
            addableComponentTypeNames.push_back(typeName);
         }
      }

      if (addableComponentTypeNames.empty()) {
         ImGui::Text("%s", Tr("追加できるコンポーネントがありません", "No addable components"));
      } else {
         editorSelectedAddComponentIndex_ = std::clamp(editorSelectedAddComponentIndex_, 0, static_cast<int>(addableComponentTypeNames.size() - 1));
         const char* selectedTypeName = addableComponentTypeNames[editorSelectedAddComponentIndex_].c_str();
         const std::string selectedDisplayName = LocalizeObjectComponentTypeName(selectedTypeName);

         const std::string typeLabel = std::string(Tr("タイプ", "Type")) + "##ComponentType";
         if (ImGui::BeginCombo(typeLabel.c_str(), selectedDisplayName.c_str())) {
            for (size_t i = 0; i < addableComponentTypeNames.size(); ++i) {
               const bool selected = (static_cast<int>(i) == editorSelectedAddComponentIndex_);
               const std::string displayLabel =
                  LocalizeObjectComponentTypeName(addableComponentTypeNames[i].c_str()) +
                  "##" +
                  addableComponentTypeNames[i];
               if (ImGui::Selectable(displayLabel.c_str(), selected)) {
                  editorSelectedAddComponentIndex_ = static_cast<int>(i);
               }
               if (selected) {
                  ImGui::SetItemDefaultFocus();
               }
            }
            ImGui::EndCombo();
         }

         const std::string addButtonLabel = std::string(Tr("追加", "Add Component")) + "##Button";
         if (ImGui::Button(addButtonLabel.c_str())) {
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

   if (editorContext && EngineContext::IsPlayModeEdit() &&
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsAnyItemActive()) {
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
   if (auto* currentScene = BaseScene::GetCurrentScene()) {
      if (auto* cameraEditor = currentScene->GetCameraEditor()) {
         cameraEditor->DrawSceneGizmos(
            EngineContext::GetActiveCamera(),
            viewportX,
            viewportY,
            viewportWidth,
            viewportHeight);
      }
   }
   editorContext->HandleViewportClickSelection(viewportX, viewportY, viewportWidth, viewportHeight);
}

void RendererEditorController::MarkActiveSceneDirty() {
   if (auto* editorContext = GetActiveEditorContext()) {
      editorContext->MarkDirty();
   }
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
      if (Texture* icon = EngineContext::GetTexture("engine/textures/editor/ic_system_folder_01_128.png")) {
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

   if (auto* materialComponent = selectedObject->GetComponent<MaterialComponent>()) {
      ImGui::SeparatorText(Tr("テクスチャアセットドロップ", "Texture Asset Drop"));
      const std::string currentTexture = materialComponent->GetTextureName(0).empty() ? Tr("<なし>", "<none>") : materialComponent->GetTextureName(0);
      ImGui::Button((std::string(Tr("スロット", "Slot")) + " 0: " + currentTexture).c_str(), ImVec2(-1.0f, 0.0f));
      if (ImGui::BeginDragDropTarget()) {
         if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_ASSET_TEXTURE")) {
            const char* assetId = static_cast<const char*>(payload->Data);
            if (assetId && payload->DataSize > kEmptyCStringPayloadSize) {
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

   ImGui::SeparatorText(Tr("パーティクルアセットドロップ", "Particle Asset Drop"));
   ImGui::Button(Tr("particle json または texture をここへドロップ", "Drop particle json or texture here"), ImVec2(-1.0f, 0.0f));
   if (!ImGui::BeginDragDropTarget()) {
      return;
   }

   if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_ASSET_PARTICLE")) {
      const char* assetId = static_cast<const char*>(payload->Data);
      if (assetId && payload->DataSize > kEmptyCStringPayloadSize) {
         particleSystem->LoadFromJson((std::filesystem::path("resources") / assetId).generic_string());
         editorContext.MarkDirty();
      }
   }

   if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_ASSET_TEXTURE")) {
      const char* assetId = static_cast<const char*>(payload->Data);
      if (assetId && payload->DataSize > kEmptyCStringPayloadSize) {
         EnsureTextureLoaded(assetId);
         particleSystem->SetTextureName(assetId);
         editorContext.MarkDirty();
      }
   }

   ImGui::EndDragDropTarget();
}

std::vector<Object*> RendererEditorController::CollectSceneObjects() const {
   return Object::GetRegisteredObjects();
}

void RendererEditorController::ResolveParentRelation(Object* object, const std::vector<Object*>& sceneObjects) const {
   if (!object) {
      return;
   }

   auto* transformComponent = object->GetComponent<TransformComponent>();
   if (!transformComponent) {
      return;
   }

   if (object->GetParentEntityId().empty() && !transformComponent->parentObjectName.empty()) {
      const auto legacyParent = std::find_if(sceneObjects.begin(), sceneObjects.end(),
         [object, transformComponent](const Object* candidate) {
            return candidate && candidate != object &&
               candidate->GetObjectName() == transformComponent->parentObjectName;
         });
      if (legacyParent != sceneObjects.end()) {
         object->SetParentEntityId((*legacyParent)->GetEntityId());
         transformComponent->parentObjectName.clear();
      }
   }

   if (object->GetParentEntityId().empty()) {
      transformComponent->useParentMatrix = false;
      transformComponent->parentMatrix = MakeIdentity4x4();
      return;
   }

   if (!Object::FindByEntityId(object->GetParentEntityId())) {
      transformComponent->useParentMatrix = false;
      transformComponent->parentMatrix = MakeIdentity4x4();
      return;
   }

   transformComponent->useParentMatrix = true;
   transformComponent->parentMatrix = object->GetParentWorldMatrix();
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

   int index = kFirstUniqueNameSuffix;
   while (true) {
      std::string withIndex = candidate + "_" + std::to_string(index++);
      if (!exists(withIndex)) {
         return withIndex;
      }
   }
}

} // namespace GameEngine

#endif
