#pragma once

#ifdef USE_IMGUI

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

class RendererEditorController {
public:
   void Initialize(AssetManager* assetManager);

   void BeginEditorFrame();
   void ShowPlayModeToolbar();
   void ShowAssetWindow();
   void ShowHierarchyWindow();
   void ShowInspectorWindow();
   void ShowSceneOverlay(float viewportX, float viewportY, float viewportWidth, float viewportHeight);

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

private:
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
   std::filesystem::path editorSceneFilePath_ = "resources/scenes/editor_scene.json";

   std::unordered_map<const Model*, std::string> editorModelAssetNames_;
   std::unordered_map<const Model*, std::string> editorModelMaterialNames_;
   std::unordered_set<std::string> editorLoadedTextureAssets_;
};

} // namespace GameEngine

#endif
