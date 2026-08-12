#pragma once

#ifdef USE_IMGUI

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace GameEngine {
class AssetManager;
class EditorSceneContext;
class Object;
class Model;
class Sprite;
class ParticleSystem;
enum class EditorAssetType;
struct EditorAssetEntry;

/// @brief レンダラーに付随するエディターウィンドウと選択状態を統括する
class RendererEditorController {
public:
   /// @brief アセット操作に使用するマネージャーを接続する
   void Initialize(AssetManager* assetManager);

   /// @brief フレーム開始時に遅延操作と選択状態を同期する
   void BeginEditorFrame();
   /// @brief 再生・停止・一時停止用ツールバーを描画する
   void ShowPlayModeToolbar();
   /// @brief アセット一覧ウィンドウを描画する
   void ShowAssetWindow();
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
   void EmitAssetDragPayload(const EditorAssetEntry& entry) const;
   bool EnsureTextureLoaded(const std::string& textureAssetId);
   void DrawSelectedObjectAssetDropTargets(EditorSceneContext& editorContext, Object* selectedObject);
   void DrawParticleAssetDropTarget(EditorSceneContext& editorContext, ParticleSystem* particleSystem);
   void ResolveParentRelation(Object* object, const std::vector<Object*>& sceneObjects) const;
   std::string BuildUniqueObjectName(const std::string& baseName, const std::vector<Object*>& sceneObjects) const;
   void RefreshSceneCatalog();
   bool CreateEditorScene(const std::string& sceneName);
   bool SetReleaseStartScene(const std::string& sceneName);

private:
   static constexpr size_t kSceneNameBufferSize = 128;

   AssetManager* assetManager_ = nullptr;

   bool editorAssetIconView_ = true;
   std::string editorNewModelName_ = "NewModel";
   std::string editorNewSpriteName_ = "NewSprite";
   std::string editorNewMaterialName_ = "NewMaterial";
   int editorSelectedModelAssetIndex_ = 0;
   int editorSelectedMaterialIndex_ = 0;
   int editorSelectedAssetMaterialIndex_ = 0;
   int editorNewMaterialLightingMode_ = 2;
   float editorNewMaterialColor_[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
   int editorSelectedAddComponentIndex_ = 0;
   std::filesystem::path editorSceneFilePath_ = "resources/game/scenes/editor_scene.json";
   char editorNewSceneName_[kSceneNameBufferSize] = "NewScene";
   std::vector<std::string> editorSceneNames_;
   std::string editorSelectedSceneName_;
   std::string editorReleaseStartSceneName_;
   std::string editorSceneCatalogStatus_;
   const Object* editorComponentSaveStatusObject_ = nullptr;
   std::string editorComponentSaveStatus_;

   std::unordered_map<const Model*, std::string> editorModelAssetNames_;
   std::unordered_map<const Model*, std::string> editorModelMaterialNames_;
   std::unordered_set<std::string> editorLoadedTextureAssets_;
};

} // namespace GameEngine

#endif
