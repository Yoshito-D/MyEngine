#pragma once

#ifdef USE_IMGUI

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace GameEngine {
class AssetManager;
class Object;
class Model;
class Sprite;

class RendererEditorController {
public:
   void Initialize(AssetManager* assetManager);

   void ShowSceneEditorWindow();
   void ShowHierarchyWindow();
   void ShowInspectorWindow();

private:
   std::vector<Object*> CollectSceneObjects() const;
   void ResolveParentRelation(Object* object, const std::vector<Object*>& sceneObjects) const;
   std::string BuildUniqueObjectName(const std::string& baseName, const std::vector<Object*>& sceneObjects) const;
   bool SaveEditorSceneToFile(const std::filesystem::path& filePath) const;
   bool LoadEditorSceneFromFile(const std::filesystem::path& filePath);

private:
   AssetManager* assetManager_ = nullptr;
   Object* selectedObject_ = nullptr;

   std::string editorNewModelName_ = "NewModel";
   std::string editorNewSpriteName_ = "NewSprite";
   int editorSelectedModelAssetIndex_ = 0;
   int editorSelectedMaterialIndex_ = 0;
   std::filesystem::path editorSceneFilePath_ = "resources/scenes/editor_scene.json";

   std::vector<std::unique_ptr<Model>> editorCreatedModels_;
   std::vector<std::unique_ptr<Sprite>> editorCreatedSprites_;
   std::unordered_map<const Model*, std::string> editorModelAssetNames_;
   std::unordered_map<const Model*, std::string> editorModelMaterialNames_;
};

} // namespace GameEngine

#endif
