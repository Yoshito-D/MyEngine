#pragma once

#ifdef USE_IMGUI

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace GameEngine {
class AssetManager;
class EditorSceneContext;
class Object;
enum class EditorAssetType;
struct EditorAssetEntry;

/// @brief レンダラーに付随するエディターウィンドウと選択状態を統括する
class RendererEditorController {
public:
   /// @brief アセット操作に使用するマネージャーを接続する
   void Initialize(AssetManager* assetManager);

   /// @brief フレーム開始時に遅延操作と選択状態を同期する
   void BeginEditorFrame();
   /// @brief 上部メニューバー内のファイル・設定メニューを描画する。
   void ShowMainMenuItems();
   /// @brief 各ウィンドウの表示状態に依存せず未保存シーンの確認を描画する。
   void ShowSceneDialogs();
   /// @brief 再生・停止・一時停止用ツールバーを描画する
   void ShowPlayModeToolbar();
   /// @brief アセット一覧ウィンドウを描画する
   void ShowAssetWindow();
   /// @brief シーン切替・保存・作成・開始シーン設定をまとめた管理ウィンドウを描画する。
   void ShowSceneManagementWindow();
   /// @brief シーン階層ウィンドウを描画する
   void ShowHierarchyWindow();
   /// @brief 選択中オブジェクトのインスペクターを描画する
   void ShowInspectorWindow();
   /// @brief シーンビューポート上の操作UIを描画する
   void ShowSceneOverlay(float viewportX, float viewportY, float viewportWidth, float viewportHeight);

   /// @brief 現在のエディタシーンを保存が必要な状態として記録する
   void MarkActiveSceneDirty();

private:
   EditorSceneContext* GetActiveEditorContext() const;
   std::vector<Object*> CollectSceneObjects() const;
   void DrawAssetEntry(EditorSceneContext& editorContext, const EditorAssetEntry& entry);
   void DrawAssetTree(EditorSceneContext& editorContext);
   void NavigateToFolder(const std::string& folderId);
   void EmitAssetDragPayload(const EditorAssetEntry& entry) const;
   void DrawAssetContextMenu(EditorSceneContext& context, const EditorAssetEntry& entry);
   void DrawAssetDialogs(EditorSceneContext& context);
   void AcceptAssetMove(EditorSceneContext& context, const std::string& folder);
   void CheckAssetOperations(EditorSceneContext& context, const std::string& id);
   void RequestSceneOpen(const std::string& sceneName, bool reload = false);
   void DrawUnsavedSceneDialog(EditorSceneContext& context);
   void HandlePanelShortcuts(EditorSceneContext& context, bool projectPanel);
   void FinishInspectorEdit(EditorSceneContext& context);
   void SelectAsset(EditorSceneContext& context, const std::string& id);
   void RefreshSceneCatalog();
   bool CreateEditorScene(const std::string& sceneName);
   bool SetReleaseStartScene(const std::string& sceneName);

private:
   static constexpr size_t kSceneNameBufferSize = 128;

   AssetManager* assetManager_ = nullptr;

   bool editorAssetIconView_ = true;
   float editorThumbnailSize_ = 64.0f;
   float editorFolderPaneWidth_ = 190.0f;
   std::string editorCurrentFolder_;
   std::string editorTreeRevealFolder_;
   std::string editorSelectedAssetId_;
   char editorAssetSearch_[256]{};
   int editorAssetTypeFilter_ = 0;
   bool editorSearchAllFolders_ = false;
   bool editorRevealAsset_ = false;
   std::string editorAssetQueryKey_;
   std::vector<std::string> editorVisibleAssets_;
   std::string editorAssetStatus_;
   std::string editorAssetOperation_;
   std::string editorAssetOperationId_;
   std::string editorAssetMoveDestination_;
   char editorAssetName_[256]{};
   std::string editorAssetCheckKey_;
   std::string editorAssetRenameReason_;
   std::string editorAssetDuplicateReason_;
   std::string editorAssetDeleteReason_;
   std::vector<std::string> editorAssetDeleteReferences_;
   bool editorAssetDeleteReferencesLoaded_ = false;
   int editorSelectedAddComponentIndex_ = 0;
   char editorNewSceneName_[kSceneNameBufferSize] = "NewScene";
   std::vector<std::string> editorSceneNames_;
   std::string editorSelectedSceneName_;
   std::string editorReleaseStartSceneName_;
   std::string editorSceneCatalogStatus_;
   bool editorSceneReloadRequested_ = false;
   std::filesystem::path editorSceneReloadFilePath_;
   std::string editorPendingSceneName_;
   bool editorPendingSceneReload_ = false;
   bool editorUnsavedDialogRequested_ = false;
   std::filesystem::path editorActiveScenePath_;
   std::string editorInspectorEntityId_;
   bool editorInspectorParticle_ = false;
   nlohmann::json editorInspectorBefore_;
   nlohmann::json editorInspectorAfter_;
   bool editorFocusName_ = false;
   std::string editorComponentSaveStatusEntityId_;
   std::string editorComponentSaveStatus_;
};

} // namespace GameEngine

#endif
