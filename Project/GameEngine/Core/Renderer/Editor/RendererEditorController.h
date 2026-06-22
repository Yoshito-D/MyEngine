#pragma once

#ifdef USE_IMGUI

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace GameEngine {
class AssetManager;
class EditorSceneContext;
class Object;
class Model;
class Sprite;
class ParticleSystem;

class RendererEditorController {
public:
   void Initialize(AssetManager* assetManager);

   void ShowAssetWindow();
   void ShowHierarchyWindow();
   void ShowInspectorWindow();
   void ShowSceneOverlay(float viewportX, float viewportY, float viewportWidth, float viewportHeight);

private:
   EditorSceneContext* GetActiveEditorContext() const;
   std::vector<Object*> CollectSceneObjects() const;
   void ResolveParentRelation(Object* object, const std::vector<Object*>& sceneObjects) const;
   std::string BuildUniqueObjectName(const std::string& baseName, const std::vector<Object*>& sceneObjects) const;
   bool SaveEditorSceneToFile(const std::filesystem::path& filePath) const;
   bool LoadEditorSceneFromFile(const std::filesystem::path& filePath);

private:
   AssetManager* assetManager_ = nullptr;
   ParticleSystem* selectedParticleSystem_ = nullptr;

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
};

} // namespace GameEngine

#endif
