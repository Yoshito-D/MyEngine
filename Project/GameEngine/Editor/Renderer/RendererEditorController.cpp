#include "GameEngine/pch.h"
#include "GameEngine/Editor/Renderer/RendererEditorController.h"

#ifdef USE_IMGUI

#include "GameEngine/Assets/AssetManager.h"
#include "GameEngine/Assets/Material/MaterialManager.h"
#include "GameEngine/Assets/Texture/TextureManager.h"
#include "GameEngine/Object/Component/Rendering/MaterialComponent.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Component/Rendering/RenderComponent.h"
#include "GameEngine/Graphics/Resources/Material.h"
#include "GameEngine/Object/Component/Base/ComponentRegistry.h"
#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Object/Model/Model.h"
#include "GameEngine/Object/Sprite/Sprite.h"
#include "GameEngine/Object/Text/UIText.h"
#include "GameEngine/Object/Skybox/Skybox.h"
#include "GameEngine/Effects/Particles/ParticleSystem.h"
#include "GameEngine/Editor/Particle/ParticleSystemEditor.h"
#include "GameEngine/Editor/EditorAssetRegistry.h"
#include "GameEngine/Editor/EditorSceneContext.h"
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Utility/JsonFile.h"
#include <shellapi.h>
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Graphics/Resources/Texture.h"
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#include "GameEngine/Scene/BaseScene.h"
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
constexpr float kHierarchyDropGuideThickness = 2.0f;
constexpr int kEmptyCStringPayloadSize = 1;
constexpr size_t kInspectorNameBufferSize = 256;
constexpr const char* kFolderIconAssetId = "engine/textures/editor/ic_system_folder_01_128.png";

void ContinueRowIfFits(float nextWidth) {
   const float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
   if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + nextWidth <= right) ImGui::SameLine();
}

std::string ParentAssetId(const std::string& id) {
   const auto separator = id.find_last_of('/');
   return separator == std::string::npos ? std::string{} : id.substr(0, separator);
}

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
   if (catalogData.contains("initialScene") && !catalogData.at("initialScene").is_string()) {
      errorMessage = "Scene catalog initialScene must be a string";
      return false;
   }
   return true;
}

bool SaveJsonFile(
   const std::filesystem::path& filePath,
   const nlohmann::json& jsonData,
   std::string& errorMessage) {
   try {
      if (SaveJsonFileAtomically(filePath, jsonData, kJsonIndentSize)) return true;
      errorMessage = "Write failed: " + filePath.generic_string();
   } catch (const std::exception& error) { errorMessage = "Write failed: " + std::string(error.what()); }
   return false;
}

bool IsValidSceneName(const std::string& sceneName) {
   return EditorAssetRegistry::IsValidName(sceneName);
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
   auto* context = GetActiveEditorContext();
   if (!context) return;
   const auto scenePath = context->GetSceneFilePath();
   if (editorActiveScenePath_ != scenePath) {
      editorActiveScenePath_ = scenePath;
      editorAssetQueryKey_.clear();
      editorAssetCheckKey_.clear();
      editorAssetOperation_.clear();
      editorInspectorEntityId_.clear();
      editorInspectorBefore_ = {};
      editorInspectorAfter_ = {};
      editorComponentSaveStatusEntityId_.clear();
      editorComponentSaveStatus_.clear();
      editorSceneReloadRequested_ = false;
   }
   context->FinishEditingFrame();
   if (editorSceneReloadRequested_) {
      editorSceneReloadRequested_ = false;
      if (editorSceneReloadFilePath_ == scenePath) {
         editorInspectorEntityId_.clear();
         editorAssetQueryKey_.clear();
         editorAssetCheckKey_.clear();
         context->Load();
      }
      editorSceneReloadFilePath_.clear();
   }
   const auto& registry = context->GetAssetRegistry();
   if (!editorSelectedAssetId_.empty() && !registry.FindAsset(editorSelectedAssetId_)) editorSelectedAssetId_.clear();
   if (!editorCurrentFolder_.empty() && !registry.FindAsset(editorCurrentFolder_, EditorAssetType::Folder)) editorCurrentFolder_.clear();
   if (std::string reveal = EditorUI::TakeAssetRevealRequest(); !reveal.empty() && registry.FindAsset(reveal)) {
      NavigateToFolder(ParentAssetId(reveal));
      editorAssetSearch_[0] = '\0';
      editorAssetTypeFilter_ = 0;
      editorSearchAllFolders_ = false;
      SelectAsset(*context, reveal);
      editorRevealAsset_ = true;
      if (bool* visible = EngineContext::GetEditorWindowVisibility("Assets", { "プロジェクト", "Project" })) *visible = true;
   }
}

void RendererEditorController::ShowMainMenuItems() {
   auto* context = GetActiveEditorContext();
   if (ImGui::BeginMenu(Tr("ファイル", "File"))) {
      const bool editable = context && EngineContext::IsPlayModeEdit();
      ImGui::BeginDisabled(!editable);
      if (ImGui::MenuItem(Tr("シーン保存", "Save Scene"), "Ctrl+S")) { FinishInspectorEdit(*context); context->Save(); }
      if (ImGui::MenuItem(Tr("シーン再読込", "Reload Scene"))) RequestSceneOpen({}, true);
      if (ImGui::BeginMenu(Tr("シーンを開く", "Open Scene"))) {
         for (const auto& name : editorSceneNames_) if (ImGui::MenuItem(name.c_str())) RequestSceneOpen(name);
         ImGui::EndMenu();
      }
      if (ImGui::BeginMenu(Tr("シーン作成", "Create Scene"))) {
         ImGui::InputText("Name", editorNewSceneName_, sizeof(editorNewSceneName_));
         if (ImGui::Button("Create") && CreateEditorScene(editorNewSceneName_)) { if (context) context->RefreshAssets(); }
         ImGui::EndMenu();
      }
      ImGui::EndDisabled();
      if (ImGui::MenuItem(Tr("一覧を更新", "Refresh Catalog"))) RefreshSceneCatalog();
      ImGui::EndMenu();
   }
   if (ImGui::BeginMenu(Tr("設定", "Settings"))) {
      ImGui::BeginDisabled(!EngineContext::IsPlayModeEdit());
      if (ImGui::BeginMenu(Tr("開始シーン", "Start Scene"))) {
         std::string selectedStartScene;
         for (const auto& name : editorSceneNames_) {
            if (ImGui::MenuItem(name.c_str(), nullptr, name == editorReleaseStartSceneName_)) selectedStartScene = name;
         }
         ImGui::EndMenu();
         if (!selectedStartScene.empty()) SetReleaseStartScene(selectedStartScene);
      }
      ImGui::EndDisabled();
      ImGui::EndMenu();
   }
}

void RendererEditorController::ShowSceneDialogs() {
   if (auto* context = GetActiveEditorContext()) DrawUnsavedSceneDialog(*context);
}

void RendererEditorController::ShowPlayModeToolbar() {
   bool* visible = EngineContext::GetEditorWindowVisibility("PlayModeToolbar", { "再生", "Toolbar" });
   if (!visible || !*visible) return;
   const std::string label = StableWindowLabel(Tr("再生", "Toolbar"), "PlayModeToolbar");
   if (!ImGui::Begin(label.c_str(), visible)) { ImGui::End(); return; }
   auto* context = GetActiveEditorContext();
   const PlayMode mode = EngineContext::GetPlayMode();
   ImGui::BeginDisabled(mode == PlayMode::Playing);
   if (ImGui::Button(mode == PlayMode::Paused ? "Resume" : "Play")) { if (context) FinishInspectorEdit(*context); EngineContext::RequestPlayModeStart(); }
   ImGui::EndDisabled();
   ContinueRowIfFits(ImGui::CalcTextSize("Stop").x + ImGui::GetStyle().FramePadding.x * 2);
   ImGui::BeginDisabled(mode == PlayMode::Edit);
   if (ImGui::Button("Stop")) EngineContext::RequestPlayModeStop();
   ImGui::EndDisabled();
   ContinueRowIfFits(ImGui::CalcTextSize("Pause").x + ImGui::GetStyle().FramePadding.x * 2);
   ImGui::BeginDisabled(mode != PlayMode::Playing);
   if (ImGui::Button("Pause")) EngineContext::RequestPlayModePause();
   ImGui::EndDisabled();
   ContinueRowIfFits(ImGui::CalcTextSize("Step").x + ImGui::GetStyle().FramePadding.x * 2);
   ImGui::BeginDisabled(mode != PlayMode::Paused);
   if (ImGui::Button("Step")) EngineContext::RequestPlayModeStep();
   ImGui::EndDisabled();
   float timeScale = EngineContext::GetTimeScale();
   if (ImGuiHelper::DrawSliderFloat(Tr("タイムスケール", "Time Scale"), timeScale, 0.0f, 2.0f)) {
      EngineContext::SetTimeScale(timeScale);
   }
   ImGui::Text("%s", EngineContext::GetPlayModeName());
   ImGui::Text("%s: %.4f", Tr("デルタタイム", "Delta Time"), EngineContext::GetDeltaTime());
   ImGui::Text("%s: %.4f", Tr("実時間デルタタイム", "Unscaled Delta Time"), EngineContext::GetUnscaledDeltaTime());
   ImGui::End();
}

void RendererEditorController::ShowSceneManagementWindow() {
   bool* visible = EngineContext::GetEditorWindowVisibility("SceneManagement", { "シーン管理", "Scene Management" });
   if (!visible || !*visible) return;
   const std::string label = StableWindowLabel(Tr("シーン管理", "Scene Management"), "SceneManagement");
   const auto* viewport = ImGui::GetMainViewport();
   const float scale = ImGui::GetFontSize() / 13.0f;
   ImGui::SetNextWindowSize(ImVec2(std::min(420.0f * scale, std::max(120.0f, viewport->WorkSize.x - 32.0f)),
      std::min(560.0f * scale, std::max(100.0f, viewport->WorkSize.y - 32.0f))), ImGuiCond_FirstUseEver);
   ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
      viewport->WorkPos.y + viewport->WorkSize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
   // 初期表示だけ浮動位置を指定し、その後のドッキングと保存済み配置はImGuiに任せる。
   if (!ImGui::Begin(label.c_str(), visible)) { ImGui::End(); return; }
   auto* context = GetActiveEditorContext();
   const auto* scene = BaseScene::GetCurrentScene();
   const bool editable = context && EngineContext::IsPlayModeEdit();
   ImGui::TextWrapped("%s: %s%s", Tr("現在のシーン", "Current scene"),
      scene ? scene->GetEditorSceneName().c_str() : "<none>", context && context->IsDirty() ? " *" : "");
   if (context) {
      const auto pathUtf8 = context->GetSceneFilePath().generic_u8string();
      const std::string path(pathUtf8.begin(), pathUtf8.end());
      ImGui::TextWrapped("%s", path.c_str());
      const auto& io = ImGui::GetIO();
      if (editable && !io.WantTextInput && !ImGui::IsAnyItemActive() &&
         ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
         FinishInspectorEdit(*context); context->Save();
      }
   }
   ImGui::BeginDisabled(!editable);
   if (ImGui::Button(Tr("保存", "Save"))) { FinishInspectorEdit(*context); context->Save(); }
   const char* reloadLabel = Tr("再読込", "Reload");
   ContinueRowIfFits(ImGui::CalcTextSize(reloadLabel).x + ImGui::GetStyle().FramePadding.x * 2);
   if (ImGui::Button(reloadLabel)) RequestSceneOpen({}, true);
   ImGui::EndDisabled();
   ImGui::Separator();
   ImGui::TextWrapped("%s", Tr("シーンを切り替える", "Switch scene"));
   ImGui::SetNextItemWidth(-1.0f);
   if (ImGui::BeginCombo("##SceneToOpen", editorSelectedSceneName_.empty() ? "<none>" : editorSelectedSceneName_.c_str())) {
      for (const auto& name : editorSceneNames_) {
         if (ImGui::Selectable(name.c_str(), name == editorSelectedSceneName_)) editorSelectedSceneName_ = name;
      }
      ImGui::EndCombo();
   }
   ImGui::BeginDisabled(!editable || editorSelectedSceneName_.empty());
   if (ImGui::Button(Tr("選択シーンを開く", "Open"), ImVec2(std::max(1.0f, ImGui::GetContentRegionAvail().x), 0))) RequestSceneOpen(editorSelectedSceneName_);
   ImGui::EndDisabled();
   if (ImGui::Button(Tr("一覧更新", "Refresh"), ImVec2(std::max(1.0f, ImGui::GetContentRegionAvail().x), 0))) RefreshSceneCatalog();
   ImGui::Separator();
   ImGui::TextWrapped("%s", Tr("新しいシーン", "New scene"));
   ImGui::BeginDisabled(!editable);
   ImGui::SetNextItemWidth(-1.0f);
   ImGui::InputText("##NewSceneName", editorNewSceneName_, sizeof(editorNewSceneName_));
   if (ImGui::Button(Tr("シーンを作成", "Create"), ImVec2(std::max(1.0f, ImGui::GetContentRegionAvail().x), 0)) && CreateEditorScene(editorNewSceneName_)) context->RefreshAssets();
   ImGui::EndDisabled();
   ImGui::Separator();
   ImGui::TextWrapped("%s", Tr("Releaseの開始シーン", "Release Start Scene"));
   ImGui::BeginDisabled(!EngineContext::IsPlayModeEdit());
   ImGui::SetNextItemWidth(-1.0f);
   if (ImGui::BeginCombo("##StartScene", editorReleaseStartSceneName_.empty() ? "<none>" : editorReleaseStartSceneName_.c_str())) {
      std::string selectedStartScene;
      for (const auto& name : editorSceneNames_) {
         if (ImGui::Selectable(name.c_str(), name == editorReleaseStartSceneName_)) selectedStartScene = name;
      }
      ImGui::EndCombo();
      if (!selectedStartScene.empty()) SetReleaseStartScene(selectedStartScene);
   }
   ImGui::EndDisabled();
   ImGui::TextWrapped("%s", Tr("Release起動時の開始シーンです。エディタは最後に開いたシーンを優先し、現在のシーンは切り替わりません。",
      "Release starts here. The editor resumes the last open scene, and the current scene stays open."));
   if (context && !context->GetLastStatusMessage().empty()) ImGui::TextWrapped("%s", context->GetLastStatusMessage().c_str());
   if (!editorSceneCatalogStatus_.empty()) ImGui::TextWrapped("%s", editorSceneCatalogStatus_.c_str());
   ImGui::End();
}

void RendererEditorController::ShowAssetWindow() {
   bool* visible = EngineContext::GetEditorWindowVisibility("Assets", { "プロジェクト", "Project" });
   if (!visible || !*visible) return;
   const std::string label = StableWindowLabel(Tr("プロジェクト", "Project"), "Assets");
   if (!ImGui::Begin(label.c_str(), visible)) { ImGui::End(); return; }
   auto* context = GetActiveEditorContext();
   if (!context) { ImGui::TextUnformatted("No editor scene"); ImGui::End(); return; }
   HandlePanelShortcuts(*context, true);
   if (ImGui::SmallButton("Rescan")) { if (assetManager_ && assetManager_->GetTextureManager()) assetManager_->GetTextureManager()->RefreshFailedLoads(); context->RefreshAssets(); editorAssetQueryKey_.clear(); }
   ContinueRowIfFits(ImGui::CalcTextSize("Grid").x + ImGui::GetStyle().FramePadding.x * 2);
   if (ImGui::SmallButton(editorAssetIconView_ ? "Grid" : "List")) editorAssetIconView_ = !editorAssetIconView_;
   const float scale = ImGui::GetFontSize() / 13.0f;
   if (editorAssetIconView_) {
      ContinueRowIfFits(100.0f * scale);
      ImGui::SetNextItemWidth(std::max(1.0f, std::min(100.0f * scale, ImGui::GetContentRegionAvail().x)));
      ImGui::SliderFloat("##Thumbnail", &editorThumbnailSize_, 32.0f, 128.0f, "%.0f px");
   }
   // 幅がある時は一行へまとめ、狭い時だけ折り返してブラウザーの高さを確保する。
   ContinueRowIfFits(220.0f * scale);
   ImGui::SetNextItemWidth(std::max(1.0f, std::min(220.0f * scale, ImGui::GetContentRegionAvail().x)));
   ImGui::InputTextWithHint("##Search", Tr("名前を検索", "Search names"), editorAssetSearch_, sizeof(editorAssetSearch_));
   const char* filters[] = { "All types", "Folder", "Model", "Texture", "Audio", "Particle", "Scene", "Material", "Prefab", "Json", "Unknown" };
   ContinueRowIfFits(125.0f * scale);
   ImGui::SetNextItemWidth(std::max(1.0f, std::min(125.0f * scale, ImGui::GetContentRegionAvail().x)));
   ImGui::Combo("##Type", &editorAssetTypeFilter_, filters, IM_ARRAYSIZE(filters));
   const char* scopes[] = { "This folder", "All Resources" };
   int scope = editorSearchAllFolders_ ? 1 : 0;
   ContinueRowIfFits(125.0f * scale);
   ImGui::SetNextItemWidth(std::max(1.0f, std::min(125.0f * scale, ImGui::GetContentRegionAvail().x)));
   if (ImGui::Combo("##SearchScope", &scope, scopes, IM_ARRAYSIZE(scopes))) editorSearchAllFolders_ = scope == 1;
   if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", editorSearchAllFolders_ ? "Search scope: all Resources" : "Search scope: current folder only");
   // パンくずは横スクロールでき、狭い幅でも各フォルダへ移動できる。
   ImGui::BeginChild("Breadcrumb", ImVec2(0, ImGui::GetFrameHeightWithSpacing() + 6), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
   if (ImGui::SmallButton("Resources")) NavigateToFolder({});
   const std::string folder = editorCurrentFolder_;
   size_t begin = 0;
   while (begin < folder.size()) {
      const auto end = folder.find('/', begin);
      const std::string part = folder.substr(begin, end == std::string::npos ? end : end - begin);
      const std::string path = folder.substr(0, end);
      ImGui::SameLine(); ImGui::TextUnformatted("/"); ImGui::SameLine();
      ImGui::PushID(path.c_str());
      if (ImGui::SmallButton(part.c_str())) NavigateToFolder(path);
      ImGui::PopID();
      if (end == std::string::npos) break;
      begin = end + 1;
   }
   ImGui::EndChild();
   const float width = ImGui::GetContentRegionAvail().x;
   const float treeWidth = std::clamp(editorFolderPaneWidth_, 60.0f, std::max(60.0f, width * 0.6f));
   const float statusHeight = editorAssetStatus_.empty() && context->GetAssetRegistry().GetScanError().empty() ? 0.0f : ImGui::GetTextLineHeightWithSpacing() * 2.0f;
   ImGui::BeginChild("Folders", ImVec2(treeWidth, -statusHeight), ImGuiChildFlags_Borders);
   DrawAssetTree(*context);
   ImGui::EndChild();
   ImGui::SameLine(0, 0);
   ImGui::InvisibleButton("FolderSplitter", ImVec2(6, std::max(1.0f, ImGui::GetContentRegionAvail().y - statusHeight)));
   if (ImGui::IsItemActive()) editorFolderPaneWidth_ = std::clamp(editorFolderPaneWidth_ + ImGui::GetIO().MouseDelta.x, 60.0f, std::max(60.0f, width * 0.6f));
   ImGui::SameLine(0, 0);
   ImGui::BeginChild("FolderContents", ImVec2(0, -statusHeight), ImGuiChildFlags_Borders);
   const auto& registry = context->GetAssetRegistry();
   const std::string key = std::to_string(registry.GetRevision()) + "|" + editorCurrentFolder_ + "|" + editorAssetSearch_ + "|" + std::to_string(editorAssetTypeFilter_) + "|" + std::to_string(editorSearchAllFolders_);
   if (key != editorAssetQueryKey_) {
      editorAssetQueryKey_ = key;
      editorVisibleAssets_.clear();
      std::string search = editorAssetSearch_;
      std::transform(search.begin(), search.end(), search.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      for (const auto& entry : registry.GetAllAssets()) {
         if (!editorSearchAllFolders_ && ParentAssetId(entry.assetId) != editorCurrentFolder_) continue;
         if (editorAssetTypeFilter_ && static_cast<int>(entry.type) != editorAssetTypeFilter_ - 1) continue;
         std::string name = entry.displayName;
         std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
         if (name.find(search) == std::string::npos) continue;
         editorVisibleAssets_.push_back(entry.assetId);
      }
      std::sort(editorVisibleAssets_.begin(), editorVisibleAssets_.end(), [&](const std::string& a, const std::string& b) {
         const auto* lhs = registry.FindAsset(a); const auto* rhs = registry.FindAsset(b);
         if ((lhs->type == EditorAssetType::Folder) != (rhs->type == EditorAssetType::Folder)) return lhs->type == EditorAssetType::Folder;
         return a < b;
      });
   }
   const float cellWidth = editorThumbnailSize_ + ImGui::GetStyle().FramePadding.x * 2.0f + 12.0f;
   const int columns = editorAssetIconView_ ? std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cellWidth)) : 1;
   if (ImGui::BeginTable("AssetContents", columns)) {
      for (const auto& id : editorVisibleAssets_) {
         if (const auto* entry = registry.FindAsset(id)) { ImGui::TableNextColumn(); DrawAssetEntry(*context, *entry); }
      }
      ImGui::EndTable();
   }
   if (ImGui::BeginPopupContextWindow("FolderCreate", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
      if (ImGui::MenuItem("Create Folder", nullptr, false, EngineContext::IsPlayModeEdit())) {
         editorAssetOperation_ = "Create Folder"; editorAssetOperationId_ = editorCurrentFolder_; std::snprintf(editorAssetName_, sizeof(editorAssetName_), "%s", "New Folder");
      }
      ImGui::EndPopup();
   }
   ImGui::EndChild();
   if (!editorAssetStatus_.empty()) ImGui::TextWrapped("%s", editorAssetStatus_.c_str());
   if (!registry.GetScanError().empty()) ImGui::TextWrapped("%s", registry.GetScanError().c_str());
   DrawAssetDialogs(*context);
   ImGui::End();
}

void RendererEditorController::ShowHierarchyWindow() {
   const std::string windowLabel = StableWindowLabel(Tr("ヒエラルキー", "Hierarchy"), "Hierarchy");
   ImGui::Begin(windowLabel.c_str());

   auto* editorContext = GetActiveEditorContext();
   if (editorContext) HandlePanelShortcuts(*editorContext, false);
   // コンテキストがある場合は削除墓標で隠されたシーン所有物を除いた編集用一覧を正本にする。
   auto sceneObjects = editorContext ? editorContext->CollectEditableObjects() : CollectSceneObjects();
   auto particleSystems = editorContext ? editorContext->CollectEditableParticleSystems() : ParticleSystem::GetRegisteredParticleSystems();

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

      const auto drawAssetMenu = [&](const char* label, EditorAssetType type) {
         if (!ImGui::BeginMenu(label)) return;
         bool any = false;
         for (const auto& entry : editorContext->GetAssetRegistry().GetAllAssets()) {
            if (entry.type != type) continue;
            any = true;
            if (ImGui::MenuItem(entry.assetId.c_str())) {
               FinishInspectorEdit(*editorContext);
               if (type == EditorAssetType::Texture) editorContext->CreateSpriteFromTexture(entry.assetId);
               else editorContext->PlaceAsset(entry.assetId);
            }
         }
         if (!any) ImGui::TextDisabled("No compatible assets");
         ImGui::EndMenu();
      };
      drawAssetMenu(Tr("モデル", "Model"), EditorAssetType::Model);
      drawAssetMenu(Tr("スプライト", "Sprite"), EditorAssetType::Texture);
      drawAssetMenu(Tr("パーティクル", "Particle System"), EditorAssetType::Particle);

      if (ImGui::MenuItem(Tr("UIテキスト", "UI Text"))) {
         editorContext->CreateUIText();
      }

      if (ImGui::MenuItem(Tr("スカイボックス", "Skybox"))) {
         editorContext->CreateSkybox();
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

      ImGui::EndPopup();
   }

   ImGui::Button("Drop Model / Particle at scene root", ImVec2(-1, 0));
   if (editorContext && ImGui::BeginDragDropTarget()) {
      if (const auto* entry = EditorUI::AcceptAssetDrop(EditorAssetType::Unknown, [](const EditorAssetEntry& entry) { return entry.type == EditorAssetType::Model || entry.type == EditorAssetType::Particle; })) {
         FinishInspectorEdit(*editorContext);
         if (editorContext->PlaceAsset(entry->assetId)) editorSelectedAssetId_.clear();
      }
      ImGui::EndDragDropTarget();
   }
   // メニューやDropの生成結果もこのフレームの選択検証へ含める。
   if (editorContext) {
      sceneObjects = editorContext->CollectEditableObjects();
      particleSystems = editorContext->CollectEditableParticleSystems();
      if (editorContext->GetSelectedObject() || editorContext->GetSelectedParticleSystem()) editorSelectedAssetId_.clear();
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
         // 再帰へ入る前に記録し、循環した親子参照でも無限再帰しないようにする。
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
            // フレームをまたぐDrag中も有効なEntity IDを渡し、生ポインターをPayloadへ保存しない。
            ImGui::SetDragDropPayload("EDITOR_SCENE_OBJECT", entityId.c_str(), entityId.size() + 1);
            ImGui::Text("%s", objectName.c_str());
            ImGui::TextDisabled("%s", Tr(
               "行間: 並び替え  オブジェクト上: 子にする",
               "Between rows: reorder  On object: make child"));
            ImGui::EndDragDropSource();
         }
         if (ImGui::BeginDragDropTarget()) {
            if (editorContext) if (const auto* entry = EditorUI::AcceptAssetDrop(EditorAssetType::Model)) {
               FinishInspectorEdit(*editorContext);
               if (editorContext->PlaceAsset(entry->assetId, object)) editorSelectedAssetId_.clear();
            }
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
            FinishInspectorEdit(*editorContext);
            editorSelectedAssetId_.clear();
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
         // 親が非表示・欠損している子もルート扱いにし、編集不能な孤児を作らない。
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
            FinishInspectorEdit(*editorContext);
            editorSelectedAssetId_.clear();
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
      editorSelectedSceneName_.clear();
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

   // 選択中の項目が外部編集で消えた場合は、起動シーンへ戻して不正な切替要求を防ぐ。
   if (std::find(editorSceneNames_.begin(), editorSceneNames_.end(), editorSelectedSceneName_) == editorSceneNames_.end()) {
      const auto* currentScene = BaseScene::GetCurrentScene();
      const std::string preferred = currentScene ? currentScene->GetEditorSceneName() : editorReleaseStartSceneName_;
      editorSelectedSceneName_ = std::find(editorSceneNames_.begin(), editorSceneNames_.end(), preferred) != editorSceneNames_.end()
         ? preferred : (editorSceneNames_.empty() ? std::string{} : editorSceneNames_.front());
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

   // Windowsの大文字小文字非区別環境で同一ファイルへ衝突しないよう、表示名もcase-insensitiveで検査する。
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
      std::filesystem::path("resources") / "game" / "scenes" / std::filesystem::path(std::u8string(sceneName.begin(), sceneName.end()) + u8".json");
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
      // カタログへ登録できなければ到達不能なシーンファイルになるため、直前の生成だけをロールバックする。
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
   if (editorContext) HandlePanelShortcuts(*editorContext, false);
   if (editorContext && !editorSelectedAssetId_.empty()) {
      if (const auto* entry = editorContext->GetAssetRegistry().FindAsset(editorSelectedAssetId_)) {
         ImGui::TextWrapped("%s", entry->displayName.c_str());
         ImGui::TextDisabled("%s", EditorAssetRegistry::GetAssetTypeLabel(entry->type));
         ImGui::TextWrapped("%s", entry->assetId.c_str());
         if (entry->type == EditorAssetType::Texture) {
            if (Texture* texture = EngineContext::GetTexture(entry->assetId)) {
               ImGui::Text("%u x %u", texture->GetWidth(), texture->GetHeight());
               if (!texture->GetMetadata().IsCubemap()) {
                  const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
                  ImGui::Image(ImTextureRef(texture->GetTextureSrvHandleGPU().ptr), ImVec2(width, width * texture->GetHeight() / std::max<size_t>(1, texture->GetWidth())));
               } else ImGui::TextUnformatted("Cubemap");
            } else ImGui::TextWrapped("Preview unavailable: texture could not be decoded or is unsupported. Rescan to retry after correcting the file.");
         }
         if ((entry->type == EditorAssetType::Model || entry->type == EditorAssetType::Particle) && ImGui::Button("Place in Scene")) { const auto id = entry->assetId; if (editorContext->PlaceAsset(id)) editorSelectedAssetId_.clear(); }
         DrawAssetContextMenu(*editorContext, *entry);
      }
      ImGui::End(); return;
   }
   Object* selectedObject = editorContext ? editorContext->GetSelectedObject() : nullptr;
   ParticleSystem* selectedParticleSystem = editorContext ? editorContext->GetSelectedParticleSystem() : nullptr;

   if (!selectedObject && !selectedParticleSystem) {
      editorComponentSaveStatusEntityId_.clear();
      editorComponentSaveStatus_.clear();
      ImGui::Text("%s", Tr("未選択", "No selection"));
      ImGui::End();
      return;
   }

   const bool particleEdit = selectedParticleSystem != nullptr;
   const auto before = particleEdit ? editorContext->GetParticleSnapshot(selectedParticleSystem) : editorContext->GetObjectSnapshot(selectedObject);
   const std::string id = before.value("id", "");
   if (!editorInspectorEntityId_.empty() && (id != editorInspectorEntityId_ || particleEdit != editorInspectorParticle_)) FinishInspectorEdit(*editorContext);
   const auto revision = editorContext->GetEditRevision();
   auto recordInspector = [&]() {
      if (!editorContext || !EngineContext::IsPlayModeEdit() || revision != editorContext->GetEditRevision()) return;
      const auto after = particleEdit ? editorContext->GetParticleSnapshot(selectedParticleSystem) : editorContext->GetObjectSnapshot(selectedObject);
      if (before != after) {
         if (editorInspectorEntityId_.empty()) { editorInspectorEntityId_ = id; editorInspectorParticle_ = particleEdit; editorInspectorBefore_ = before; }
         editorInspectorAfter_ = after;
      }
      if (!ImGui::IsAnyItemActive()) FinishInspectorEdit(*editorContext);
   };
   if (selectedParticleSystem) {
      editorComponentSaveStatusEntityId_.clear();
      editorComponentSaveStatus_.clear();
      std::string particleSystemName = selectedParticleSystem->GetName();
      char particleSystemNameBuffer[kInspectorNameBufferSize]{};
      {
         const size_t copySize = std::min(particleSystemName.size(), sizeof(particleSystemNameBuffer) - 1);
         std::memcpy(particleSystemNameBuffer, particleSystemName.c_str(), copySize);
      }
      if (editorFocusName_) { ImGui::SetKeyboardFocusHere(); editorFocusName_ = false; }
      if (ImGui::InputText(Tr("名前", "Name"), particleSystemNameBuffer, sizeof(particleSystemNameBuffer))) {
         selectedParticleSystem->SetName(particleSystemNameBuffer);
      }
      if (editorContext && editorContext->CanDeleteParticleSystem(selectedParticleSystem)) {
         if (ImGui::Button(Tr("削除", "Delete"))) {
            recordInspector(); FinishInspectorEdit(*editorContext);
            editorContext->DeleteParticleSystem(selectedParticleSystem);
            ImGui::End();
            return;
         }
      }
      if (editorContext) {
         editorContext->DrawGizmoInspectorControls();
      }
      ImGui::Spacing();

      ParticleSystemEditor::Edit(selectedParticleSystem);
      recordInspector();
      ImGui::End();
      return;
   }

   // 前の選択対象に対する保存結果を別Objectのメッセージとして表示しない。
   if (editorComponentSaveStatusEntityId_ != selectedObject->GetEntityId()) {
      editorComponentSaveStatusEntityId_ = selectedObject->GetEntityId();
      editorComponentSaveStatus_.clear();
   }

   std::string objectName = selectedObject->GetObjectName();
   char objectNameBuffer[kInspectorNameBufferSize]{};
   {
      const size_t copySize = std::min(objectName.size(), sizeof(objectNameBuffer) - 1);
      std::memcpy(objectNameBuffer, objectName.c_str(), copySize);
   }
   if (editorFocusName_) { ImGui::SetKeyboardFocusHere(); editorFocusName_ = false; }
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
            recordInspector(); FinishInspectorEdit(*editorContext);
            editorContext->DeleteSelectedObject();
            ImGui::End();
            return;
         }
      }
      ImGui::SameLine();
      if (ImGui::Button(Tr("複製", "Duplicate"))) {
         recordInspector(); FinishInspectorEdit(*editorContext);
         editorContext->DuplicateSelectedObject();
      }
      ImGui::Spacing();
   }

   // Inspector描画中にComponentコンテナを変更せず、描画完了後に要求を処理してiteratorを安定させる。
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
         recordInspector(); FinishInspectorEdit(*editorContext);
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
               recordInspector(); FinishInspectorEdit(*editorContext);
               editorContext->AddComponentToSelectedObject(addableComponentTypeNames[editorSelectedAddComponentIndex_]);
            } else {
               selectedObject->AddComponentByTypeName(addableComponentTypeNames[editorSelectedAddComponentIndex_]);
            }
         }
      }
   }
   ImGui::PopID();

   ImGui::Spacing();
   recordInspector();

   ImGui::End();
}

void RendererEditorController::ShowSceneOverlay(float viewportX, float viewportY, float viewportWidth, float viewportHeight) {
   auto* editorContext = GetActiveEditorContext();
   if (!editorContext) {
      return;
   }

   // Drop、Gizmo、クリック選択の順に処理し、同じクリックで生成直後の選択が上書きされないようにする。
   HandlePanelShortcuts(*editorContext, false);
   if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      const auto mouse = ImGui::GetMousePos();
      if (mouse.x >= viewportX && mouse.x <= viewportX + viewportWidth && mouse.y >= viewportY && mouse.y <= viewportY + viewportHeight) { FinishInspectorEdit(*editorContext); editorSelectedAssetId_.clear(); }
   }
   editorContext->AcceptViewportAssetDrop();
   if (editorContext->GetSelectedObject() || editorContext->GetSelectedParticleSystem()) editorSelectedAssetId_.clear();
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

void RendererEditorController::DrawAssetEntry(EditorSceneContext& context, const EditorAssetEntry& entry) {
   ImGui::PushID(entry.assetId.c_str());
   const bool selected = entry.assetId == editorSelectedAssetId_;
   const char* type = EditorAssetRegistry::GetAssetTypeLabel(entry.type);
   const float itemHeight = editorAssetIconView_ ? editorThumbnailSize_ + ImGui::GetTextLineHeightWithSpacing() * 2.0f : ImGui::GetFrameHeight();
   const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
   const ImVec2 start = ImGui::GetCursorScreenPos();
   const std::string text = editorAssetIconView_ || entry.type == EditorAssetType::Folder
      ? "##Asset" : std::string("[") + type + "] " + entry.displayName;
   const bool clicked = ImGui::Selectable(text.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(width, itemHeight));
   const bool hovered = ImGui::IsItemHovered();
   const bool doubleClicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
   if (clicked) SelectAsset(context, entry.assetId);
   if (hovered) ImGui::SetTooltip("%s\n%s", entry.assetId.c_str(), type);
   if (selected && editorRevealAsset_) { ImGui::SetScrollHereY(); editorRevealAsset_ = false; }
   EmitAssetDragPayload(entry);
   if (entry.type == EditorAssetType::Folder) AcceptAssetMove(context, entry.assetId);
   DrawAssetContextMenu(context, entry);
   if (editorAssetIconView_) {
      auto* draw = ImGui::GetWindowDrawList();
      Texture* texture = entry.type == EditorAssetType::Folder ? EngineContext::GetTexture(kFolderIconAssetId)
         : (entry.type == EditorAssetType::Texture ? EngineContext::GetTexture(entry.assetId) : nullptr);
      const ImVec2 iconMin(start.x + std::max(0.0f, (width - editorThumbnailSize_) * 0.5f), start.y + 2.0f);
      const ImVec2 iconMax(iconMin.x + editorThumbnailSize_, iconMin.y + editorThumbnailSize_);
      if (texture && !texture->GetMetadata().IsCubemap()) draw->AddImage(ImTextureRef(texture->GetTextureSrvHandleGPU().ptr), iconMin, iconMax);
      else { draw->AddRectFilled(iconMin, iconMax, ImGui::GetColorU32(ImGuiCol_FrameBg), 4.0f); draw->AddText(ImVec2(iconMin.x + 4, iconMin.y + 4), ImGui::GetColorU32(ImGuiCol_TextDisabled), type); }
      const auto name = TruncateTextWithEllipsis(entry.displayName, width);
      draw->AddText(ImVec2(start.x, iconMax.y + 2), ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
   } else if (entry.type == EditorAssetType::Folder) {
      auto* draw = ImGui::GetWindowDrawList();
      const float iconSize = ImGui::GetFontSize();
      const float y = start.y + (itemHeight - iconSize) * 0.5f;
      Texture* icon = EngineContext::GetTexture(kFolderIconAssetId);
      const float textX = icon ? start.x + iconSize + ImGui::GetStyle().ItemInnerSpacing.x : start.x;
      if (icon) draw->AddImage(ImTextureRef(icon->GetTextureSrvHandleGPU().ptr), ImVec2(start.x, y), ImVec2(start.x + iconSize, y + iconSize));
      const auto name = TruncateTextWithEllipsis(entry.displayName, std::max(0.0f, start.x + width - textX));
      draw->AddText(ImVec2(textX, y), ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
   }
   if (doubleClicked) {
      if (entry.type == EditorAssetType::Folder) NavigateToFolder(entry.assetId);
      else if (entry.type == EditorAssetType::Scene) {
         nlohmann::json catalog; std::string error;
         if (LoadSceneCatalogData(catalog, error)) {
            bool found = false;
            for (const auto& scene : catalog["scenes"].items()) if (scene.value().is_string() && scene.value().get<std::string>() == entry.assetId) { RequestSceneOpen(scene.key()); found = true; break; }
            if (!found) editorAssetStatus_ = "This scene is not registered in scene_catalog.json";
         } else editorAssetStatus_ = error;
      }
   }
   ImGui::PopID();
}

void RendererEditorController::NavigateToFolder(const std::string& folderId) {
   editorCurrentFolder_ = folderId;
   editorTreeRevealFolder_ = folderId;
}

void RendererEditorController::DrawAssetTree(EditorSceneContext& context) {
   if (ImGui::Selectable("Resources", editorCurrentFolder_.empty())) NavigateToFolder({});
   AcceptAssetMove(context, {});
   const auto& registry = context.GetAssetRegistry();
   const auto& assets = registry.GetAllAssets();
   // 明示的な移動時だけ祖先を一度開く。選択中でも、ユーザーが閉じた状態を上書きしない。
   std::string revealFolder;
   revealFolder.swap(editorTreeRevealFolder_);
   Texture* folderIcon = EngineContext::GetTexture(kFolderIconAssetId);
   const ImTextureRef icon(folderIcon ? folderIcon->GetTextureSrvHandleGPU().ptr : 0);
   std::function<void(const std::string&)> draw = [&](const std::string& parent) {
      for (const auto index : registry.GetChildren(parent)) {
         const auto& entry = assets[index];
         if (entry.type != EditorAssetType::Folder) continue;
         ImGui::PushID(entry.assetId.c_str());
         const bool open = ImGuiHelper::DrawFolderTreeNode(entry.displayName.c_str(), editorCurrentFolder_ == entry.assetId,
            revealFolder.starts_with(entry.assetId + "/"), icon);
         if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) editorCurrentFolder_ = entry.assetId;
         if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", entry.assetId.c_str());
         EmitAssetDragPayload(entry);
         AcceptAssetMove(context, entry.assetId);
         DrawAssetContextMenu(context, entry);
         if (open) { draw(entry.assetId); ImGui::TreePop(); }
         ImGui::PopID();
      }
   };
   draw({});
}

void RendererEditorController::EmitAssetDragPayload(const EditorAssetEntry& entry) const {
   if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
      ImGui::SetDragDropPayload("EDITOR_ASSET", entry.assetId.c_str(), entry.assetId.size() + 1);
      ImGui::Text("[%s] %s", EditorAssetRegistry::GetAssetTypeLabel(entry.type), entry.assetId.c_str());
      if (entry.type == EditorAssetType::Model || entry.type == EditorAssetType::Particle) ImGui::TextUnformatted("Scene: 8 units in front of camera. Hierarchy: model becomes child at local origin.");
      ImGui::EndDragDropSource();
   }
}

std::vector<Object*> RendererEditorController::CollectSceneObjects() const {
   return Object::GetRegisteredObjects();
}

void RendererEditorController::SelectAsset(EditorSceneContext& context, const std::string& id) {
   FinishInspectorEdit(context);
   editorSelectedAssetId_ = id;
   context.SelectObject(nullptr);
   context.SelectParticleSystem(nullptr);
}

void RendererEditorController::FinishInspectorEdit(EditorSceneContext& context) {
   if (editorInspectorEntityId_.empty()) return;
   if (editorInspectorBefore_ != editorInspectorAfter_ && !editorInspectorAfter_.is_null()) {
      if (editorInspectorParticle_) context.CommitParticleEdit(editorInspectorEntityId_, editorInspectorBefore_, editorInspectorAfter_);
      else context.CommitObjectEdit(editorInspectorEntityId_, editorInspectorBefore_, editorInspectorAfter_);
   }
   editorInspectorEntityId_.clear(); editorInspectorBefore_ = {}; editorInspectorAfter_ = {};
}

void RendererEditorController::HandlePanelShortcuts(EditorSceneContext& context, bool projectPanel) {
   const auto& io = ImGui::GetIO();
   if (!EngineContext::IsPlayModeEdit() || io.WantTextInput || ImGui::IsAnyItemActive() || !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;
   if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) { FinishInspectorEdit(context); context.Save(); }
   if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) { FinishInspectorEdit(context); context.Undo(); }
   if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) { FinishInspectorEdit(context); context.Redo(); }
   if (projectPanel) {
      if (editorSelectedAssetId_.empty()) return;
      if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) { editorAssetOperation_ = "Duplicate"; editorAssetOperationId_ = editorSelectedAssetId_; }
      if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) { editorAssetOperation_ = "Delete"; editorAssetOperationId_ = editorSelectedAssetId_; }
      if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) { editorAssetOperation_ = "Rename"; editorAssetOperationId_ = editorSelectedAssetId_; const auto name = editorSelectedAssetId_.substr(editorSelectedAssetId_.find_last_of('/') + 1); if (name.size() < sizeof(editorAssetName_)) std::snprintf(editorAssetName_, sizeof(editorAssetName_), "%s", name.c_str()); }
   } else if (editorSelectedAssetId_.empty()) {
      if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) { FinishInspectorEdit(context); context.DuplicateSelectedObject(); }
      if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) { FinishInspectorEdit(context); context.DeleteSelection(); }
      if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) { editorFocusName_ = true; ImGui::SetWindowFocus(StableWindowLabel(Tr("インスペクター", "Inspector"), "Inspector").c_str()); }
   }
}

void RendererEditorController::CheckAssetOperations(EditorSceneContext& context, const std::string& id) {
   auto& registry = context.GetAssetRegistry();
   const auto scenePath = context.GetSceneFilePath().generic_u8string();
   const std::string key = id + "\n" + std::to_string(registry.GetRevision()) + "\n" + std::string(scenePath.begin(), scenePath.end());
   if (key == editorAssetCheckKey_) return;
   editorAssetCheckKey_ = key;
   registry.CanRenameOrMove(id, editorAssetRenameReason_);
   registry.CanDuplicate(id, editorAssetDuplicateReason_);
   registry.CanRemove(id, editorAssetDeleteReason_);
   const auto* entry = registry.FindAsset(id);
   if (entry && editorAssetRenameReason_.empty()) {
      const std::string scene = context.SerializeToJson().dump();
      if (scene.find(nlohmann::json(id).dump()) != std::string::npos ||
         scene.find(nlohmann::json(entry->displayName).dump()) != std::string::npos) {
         editorAssetRenameReason_ = "Referenced by the active scene; changing this path ID cannot update all references safely.";
      }
   }
   editorAssetDeleteReferences_.clear();
   editorAssetDeleteReferencesLoaded_ = false;
}

void RendererEditorController::DrawAssetContextMenu(EditorSceneContext& context, const EditorAssetEntry& entry) {
   if (!ImGui::BeginPopupContextItem("AssetActions")) return;
   // ファイル参照の調査はポップアップを開いた時だけ。描画ごとの読込を避ける。
   if (ImGui::IsWindowAppearing()) { editorAssetCheckKey_.clear(); editorAssetStatus_.clear(); }
   CheckAssetOperations(context, entry.assetId);
   ImGui::TextWrapped("%s", entry.assetId.c_str());
   if (ImGui::MenuItem("Show in Explorer")) {
      const auto absolute = std::filesystem::absolute(entry.filePath);
      const auto argument = L"/select,\"" + absolute.wstring() + L"\"";
      if (reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", L"explorer.exe", argument.c_str(), nullptr, SW_SHOWNORMAL)) <= 32) editorAssetStatus_ = "Explorer could not be opened";
   }
   const bool editable = EngineContext::IsPlayModeEdit();
   ImGui::BeginDisabled(!editable);
   const bool builtIn = entry.assetId == "engine" || entry.assetId.starts_with("engine/");
   if (entry.type == EditorAssetType::Folder) {
      if (ImGui::MenuItem("Create Folder", nullptr, false, !builtIn)) {
         editorAssetOperation_ = "Create Folder";
         editorAssetOperationId_ = entry.assetId;
         std::snprintf(editorAssetName_, sizeof(editorAssetName_), "%s", "New Folder");
      }
      if (builtIn && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Engine resource folders use built-in names.");
   }
   if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, editorAssetDuplicateReason_.empty())) { editorAssetOperation_ = "Duplicate"; editorAssetOperationId_ = entry.assetId; }
   if (!editorAssetDuplicateReason_.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", editorAssetDuplicateReason_.c_str());
   const auto filename = entry.filePath.filename().u8string();
   const std::string name(filename.begin(), filename.end());
   const bool canRename = !context.IsDirty() && !context.HasEditHistory() && editorAssetRenameReason_.empty() && name.size() < sizeof(editorAssetName_);
   if (ImGui::MenuItem("Rename", "F2", false, canRename)) {
      editorAssetOperation_ = "Rename";
      editorAssetOperationId_ = entry.assetId;
      std::snprintf(editorAssetName_, sizeof(editorAssetName_), "%s", name.c_str());
   }
   if (!canRename && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      const char* reason = (context.IsDirty() || context.HasEditHistory())
         ? "Save and reload the scene to release Undo references before modifying path IDs"
         : (name.size() >= sizeof(editorAssetName_) ? "This filename exceeds the supported edit field length." : editorAssetRenameReason_.c_str());
      ImGui::SetTooltip("%s", reason);
   }
   if (ImGui::MenuItem("Delete", "Delete", false, editorAssetDeleteReason_.empty())) { editorAssetOperation_ = "Delete"; editorAssetOperationId_ = entry.assetId; }
   if (!editorAssetDeleteReason_.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", editorAssetDeleteReason_.c_str());
   ImGui::EndDisabled();
   ImGui::EndPopup();
}

void RendererEditorController::AcceptAssetMove(EditorSceneContext& context, const std::string& folder) {
   if (!ImGui::BeginDragDropTarget()) return;
   if (const auto* payload = ImGui::AcceptDragDropPayload("EDITOR_ASSET", ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
      const auto* entry = context.GetAssetRegistry().ResolveAssetPayload(payload->Data, payload->DataSize);
      if (entry) CheckAssetOperations(context, entry->assetId);
      const size_t separator = entry ? entry->assetId.find_last_of('/') : std::string::npos;
      const std::string parent = entry && separator != std::string::npos ? entry->assetId.substr(0, separator) : "";
      const bool folderValid = folder.empty() || context.GetAssetRegistry().FindAsset(folder, EditorAssetType::Folder);
      const bool builtInTarget = folder == "engine" || folder.starts_with("engine/");
      const bool particleFolder = folder == "game/particles" || folder.starts_with("game/particles/") || folder == "particles" || folder.starts_with("particles/");
      const bool typeValid = !entry || entry->type != EditorAssetType::Particle || particleFolder;
      const bool valid = entry && folderValid && !builtInTarget && typeValid && EngineContext::IsPlayModeEdit() && !context.IsDirty() && !context.HasEditHistory() && entry->assetId != folder && parent != folder && editorAssetRenameReason_.empty();
      if (valid && payload->IsPreview()) ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 0, 0, 2);
      if (payload->IsDelivery()) {
         if (valid) {
            editorAssetOperation_ = "Move";
            editorAssetOperationId_ = entry->assetId;
            const std::string filename = separator == std::string::npos ? entry->assetId : entry->assetId.substr(separator + 1);
            editorAssetMoveDestination_ = folder.empty() ? filename : folder + "/" + filename;
         } else if (context.IsDirty() || context.HasEditHistory()) editorAssetStatus_ = "Save and reload the scene to release Undo references before moving path IDs";
         else if (builtInTarget) editorAssetStatus_ = "Engine resource folders use built-in names and cannot be modified.";
         else if (!typeValid) editorAssetStatus_ = "Particle settings must stay inside the particles resource folder.";
         else editorAssetStatus_ = entry && !editorAssetRenameReason_.empty() ? editorAssetRenameReason_ : "Invalid destination";
      }
   }
   ImGui::EndDragDropTarget();
}

void RendererEditorController::DrawAssetDialogs(EditorSceneContext& context) {
   // Registryの再構築は描画が終わった後だけ行い、走査中のEntry参照を失効させない。
   if (!editorAssetOperation_.empty()) ImGui::OpenPopup("Asset Operation");
   const float fontScale = ImGui::GetFontSize() / 13.0f;
   ImGui::SetNextWindowSize(ImVec2(std::min(560.0f * fontScale, std::max(200.0f, ImGui::GetIO().DisplaySize.x - 32.0f)), 0.0f), ImGuiCond_Appearing);
   if (!ImGui::BeginPopupModal("Asset Operation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
   if (ImGui::IsWindowAppearing()) { editorAssetCheckKey_.clear(); editorAssetStatus_.clear(); }
   CheckAssetOperations(context, editorAssetOperationId_);
   ImGui::TextWrapped("%s: %s", editorAssetOperation_.c_str(), editorAssetOperationId_.c_str());
   if (editorAssetOperation_ == "Rename" || editorAssetOperation_ == "Create Folder") ImGui::InputText("Name", editorAssetName_, sizeof(editorAssetName_));
   if (editorAssetOperation_ == "Move") ImGui::TextWrapped("Destination: %s", editorAssetMoveDestination_.c_str());
   if (editorAssetOperation_ == "Delete") {
      if (!editorAssetDeleteReferencesLoaded_) {
         editorAssetDeleteReferences_ = context.GetAssetRegistry().FindReferences(editorAssetOperationId_);
         editorAssetDeleteReferencesLoaded_ = true;
         const auto* asset = context.GetAssetRegistry().FindAsset(editorAssetOperationId_);
         const std::string scene = context.SerializeToJson().dump();
         if (scene.find(nlohmann::json(editorAssetOperationId_).dump()) != std::string::npos ||
            (asset && scene.find(nlohmann::json(asset->displayName).dump()) != std::string::npos)) {
            editorAssetDeleteReferences_.push_back("Active scene (including unsaved edits)");
         }
      }
      ImGui::TextWrapped("Delete from disk? This operation cannot be undone. References below will be missing after reload.");
      ImGui::BeginChild("DeleteReferences", ImVec2(0.0f, 120.0f * fontScale), true);
      for (const auto& reference : editorAssetDeleteReferences_) { ImGui::Bullet(); ImGui::SameLine(); ImGui::TextWrapped("%s", reference.c_str()); }
      ImGui::EndChild();
      ImGui::TextWrapped("The active scene and loaded resources can retain cached data until reload.");
   }
   std::string restriction;
   if (!EngineContext::IsPlayModeEdit()) restriction = "Asset changes are only available in Edit mode.";
   else if (editorAssetOperation_ == "Duplicate") restriction = editorAssetDuplicateReason_;
   else if (editorAssetOperation_ == "Delete") restriction = editorAssetDeleteReason_;
   else if (editorAssetOperation_ == "Create Folder" && (editorAssetOperationId_ == "engine" || editorAssetOperationId_.starts_with("engine/"))) restriction = "Engine resource folders use built-in names and cannot be modified.";
   else if (editorAssetOperation_ == "Rename" || editorAssetOperation_ == "Move") {
      restriction = (context.IsDirty() || context.HasEditHistory())
         ? "Save and reload the scene to release Undo references before changing path IDs." : editorAssetRenameReason_;
      const auto* asset = context.GetAssetRegistry().FindAsset(editorAssetOperationId_);
      if (editorAssetOperation_ == "Rename" && asset && asset->filePath.filename().u8string().size() >= sizeof(editorAssetName_)) restriction = "This filename exceeds the supported edit field length.";
   }
   if ((editorAssetOperation_ == "Rename" || editorAssetOperation_ == "Create Folder") && !EditorAssetRegistry::IsValidName(editorAssetName_)) restriction = "Enter a valid filename; reserved names and path separators are not allowed.";
   if (!restriction.empty()) ImGui::TextWrapped("%s", restriction.c_str());
   ImGui::BeginDisabled(!restriction.empty());
   if (ImGui::Button(editorAssetOperation_ == "Delete" ? "Delete permanently" : "Apply")) {
      auto& registry = context.GetAssetRegistry();
      std::string newId, error;
      bool ok = false;
      if (editorAssetOperation_ == "Create Folder") ok = registry.CreateFolder(editorAssetOperationId_, editorAssetName_, newId, error);
      else if (editorAssetOperation_ == "Duplicate") ok = registry.Duplicate(editorAssetOperationId_, newId, error);
      else if (editorAssetOperation_ == "Delete") ok = registry.Remove(editorAssetOperationId_, error);
      else {
         const size_t separator = editorAssetOperationId_.find_last_of('/');
         newId = editorAssetOperation_ == "Move" ? editorAssetMoveDestination_ : (separator == std::string::npos ? std::string(editorAssetName_) : editorAssetOperationId_.substr(0, separator + 1) + editorAssetName_);
         if (context.IsDirty() || context.HasEditHistory()) error = "Save and reload the scene to release Undo references before changing asset path IDs";
         else {
            const auto scene = context.SerializeToJson().dump();
            const auto* asset = registry.FindAsset(editorAssetOperationId_);
            if (scene.find(nlohmann::json(editorAssetOperationId_).dump()) != std::string::npos || (asset && scene.find(nlohmann::json(asset->displayName).dump()) != std::string::npos)) error = "Referenced by active scene; path reference updates cannot be guaranteed";
            else ok = registry.RenameOrMove(editorAssetOperationId_, newId, error);
         }
      }
      editorAssetStatus_ = ok ? editorAssetOperation_ + " completed" : error;
      if (ok) {
         if (assetManager_ && assetManager_->GetTextureManager()) assetManager_->GetTextureManager()->RefreshFailedLoads();
         editorAssetQueryKey_.clear(); editorAssetCheckKey_.clear();
         if (!newId.empty()) {
            const size_t separator = newId.find_last_of('/');
            NavigateToFolder(separator == std::string::npos ? "" : newId.substr(0, separator));
            SelectAsset(context, newId);
         } else if (editorSelectedAssetId_ == editorAssetOperationId_) editorSelectedAssetId_.clear();
         editorAssetOperation_.clear(); ImGui::CloseCurrentPopup();
      }
   }
   ImGui::EndDisabled();
   ContinueRowIfFits(ImGui::CalcTextSize("Cancel").x + ImGui::GetStyle().FramePadding.x * 2);
   if (ImGui::Button("Cancel")) { editorAssetOperation_.clear(); ImGui::CloseCurrentPopup(); }
   if (!editorAssetStatus_.empty()) ImGui::TextWrapped("%s", editorAssetStatus_.c_str());
   ImGui::EndPopup();
}

void RendererEditorController::RequestSceneOpen(const std::string& name, bool reload) {
   auto* context = GetActiveEditorContext();
   if (!context || !EngineContext::IsPlayModeEdit()) return;
   FinishInspectorEdit(*context);
   const auto* scene = BaseScene::GetCurrentScene();
   editorPendingSceneName_ = name;
   editorPendingSceneReload_ = reload || (scene && name == scene->GetEditorSceneName());
   if (context->IsDirty()) editorUnsavedDialogRequested_ = true;
   else if (editorPendingSceneReload_) { editorSceneReloadRequested_ = true; editorSceneReloadFilePath_ = context->GetSceneFilePath(); }
   else EngineContext::ChangeScene(name);
}

void RendererEditorController::DrawUnsavedSceneDialog(EditorSceneContext& context) {
   if (editorUnsavedDialogRequested_) { ImGui::OpenPopup("Unsaved Scene"); editorUnsavedDialogRequested_ = false; }
   if (!ImGui::BeginPopupModal("Unsaved Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
   ImGui::TextWrapped("Save changes before opening the scene?");
   const auto open = [&]() {
      editorInspectorEntityId_.clear();
      if (editorPendingSceneReload_) { editorSceneReloadRequested_ = true; editorSceneReloadFilePath_ = context.GetSceneFilePath(); }
      else EngineContext::ChangeScene(editorPendingSceneName_);
      ImGui::CloseCurrentPopup();
   };
   if (ImGui::Button("Save")) { FinishInspectorEdit(context); if (context.Save()) open(); }
   ImGui::SameLine(); if (ImGui::Button("Discard")) open();
   ImGui::SameLine(); if (ImGui::Button("Cancel")) { editorPendingSceneName_.clear(); ImGui::CloseCurrentPopup(); }
   if (!context.GetLastStatusMessage().empty()) ImGui::TextWrapped("%s", context.GetLastStatusMessage().c_str());
   ImGui::EndPopup();
}

} // namespace GameEngine

#endif
