#pragma once

#include <cstdint>
#ifdef USE_IMGUI

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace GameEngine {
class AssetManager;
class EditorSceneContext;
class EditorSceneSession;
class Object;
enum class EditorAssetType;
struct EditorAssetEntry;

/// @brief レンダラーに付随するエディターウィンドウと選択状態を統括する
class RendererEditorController {
public:
   /// @brief アセット操作に使用するマネージャーを接続する
   void Initialize(AssetManager* assetManager);

   /// @brief アプリケーションが所有するエディタ資源を関連付ける。
   void SetSession(EditorSceneSession* session);

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
   static constexpr size_t kSceneNameBufferSize = 128; ///< シーン名入力バッファの容量（終端文字を含む）。

   EditorSceneSession* session_ = nullptr; ///< 表示中シーンの編集セッションへの非所有参照。
   AssetManager* assetManager_ = nullptr;  ///< アセットの読み込みと共有を委譲する管理オブジェクトへの参照。

   bool editorAssetIconView_ = true;                      ///< アセットをアイコン一覧で表示するかどうか。
   float editorThumbnailSize_ = 64.0f;                    ///< アセット一覧のサムネイル表示サイズ（ピクセル）。
   float editorFolderPaneWidth_ = 190.0f;                 ///< アセットブラウザーのフォルダーペイン幅（ピクセル）。
   std::string editorCurrentFolder_;                      ///< アセット一覧で表示しているフォルダー。
   std::string editorTreeRevealFolder_;                   ///< フォルダーツリーで次に展開・表示する対象。
   std::string editorSelectedAssetId_;                    ///< アセットブラウザーで選択中の永続アセットID。
   char editorAssetSearch_[256]{};                        ///< アセット検索の入力文字列バッファ。
   std::int32_t editorAssetTypeFilter_ = 0;               ///< アセット一覧へ適用する種類フィルターの選択番号。
   bool editorSearchAllFolders_ = false;                  ///< 現在のフォルダー以外も検索対象に含めるかどうか。
   bool editorRevealAsset_ = false;                       ///< 選択アセットの位置へ一覧をスクロールする要求。
   std::string editorAssetQueryKey_;                      ///< 一覧の再検索が必要か判定する検索条件キー。
   std::vector<std::string> editorVisibleAssets_;         ///< 現在の検索条件に一致する表示対象のアセットID。
   std::string editorAssetStatus_;                        ///< アセット操作の結果をユーザーへ伝える表示文字列。
   std::string editorAssetOperation_;                     ///< 確認ダイアログで処理するアセット操作の種類。
   std::string editorAssetOperationId_;                   ///< 確認待ちのアセット操作の対象ID。
   std::string editorAssetMoveDestination_;               ///< 移動先として指定したアセットフォルダー。
   char editorAssetName_[256]{};                          ///< アセットの作成・改名に使う名前入力バッファ。
   std::string editorAssetCheckKey_;                      ///< 操作可否の検証結果を再利用するための条件キー。
   std::string editorAssetRenameReason_;                  ///< 改名できない理由または検証結果。
   std::string editorAssetDuplicateReason_;               ///< 複製できない理由または検証結果。
   std::string editorAssetDeleteReason_;                  ///< 削除できない理由または検証結果。
   std::vector<std::string> editorAssetDeleteReferences_; ///< 削除対象のアセットを参照している保存先の一覧。
   bool editorAssetDeleteReferencesLoaded_ = false;       ///< 削除確認用の参照元一覧を取得済みかどうか。
   std::int32_t editorSelectedAddComponentIndex_ = 0;     ///< 追加するコンポーネント候補の選択番号。
   char editorNewSceneName_[kSceneNameBufferSize] = "NewScene"; ///< 新規シーン名の入力バッファ。
   std::vector<std::string> editorSceneNames_;                  ///< シーン選択メニューに表示する登録名の一覧。
   std::string editorSelectedSceneName_;                        ///< シーン一覧で選択中の名前。
   std::string editorReleaseStartSceneName_;                    ///< 製品版の起動シーンとして選択した名前。
   std::string editorSceneCatalogStatus_;                       ///< シーンカタログ操作の結果を表示する文字列。
   bool editorSceneReloadRequested_ = false;                    ///< シーンファイルの再読み込みを要求しているかどうか。
   std::filesystem::path editorSceneReloadFilePath_;            ///< 再読み込み要求の対象シーンファイル。
   std::string editorPendingSceneName_;                         ///< 未保存確認後に読み込む予定のシーン名。
   bool editorPendingSceneReload_ = false;                      ///< 保留中の遷移が現在シーンの再読み込みかどうか。
   bool editorUnsavedDialogRequested_ = false;                  ///< 未保存の変更を扱う確認ダイアログの表示要求。
   std::filesystem::path editorActiveScenePath_;                ///< 現在編集中のシーンの保存先。
   std::string editorInspectorEntityId_;                        ///< インスペクターの変更履歴を記録する対象Entity ID。
   bool editorInspectorParticle_ = false;                       ///< インスペクターの編集対象がParticleSystemかどうか。
   nlohmann::json editorInspectorBefore_;                       ///< インスペクター編集開始時の保存状態。
   nlohmann::json editorInspectorAfter_;                        ///< インスペクター編集反映後の保存状態。
   bool editorFocusName_ = false;                               ///< 次回表示で名前入力欄へフォーカスを移す要求。
   std::string editorComponentSaveStatusEntityId_; ///< プレイ中のコンポーネント保存結果を表示する対象Entity ID。
   std::string editorComponentSaveStatus_;         ///< プレイ中のコンポーネント保存結果の表示文字列。
};

} // namespace GameEngine

#endif
