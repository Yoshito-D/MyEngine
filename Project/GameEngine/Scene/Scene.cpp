#include "GameEngine/pch.h"
#include "GameEngine/Scene/Scene.h"
#include "GameEngine/Assets/AssetManager.h"
#include "GameEngine/Assets/Texture/SceneTexturePreloader.h"
#include "GameEngine/Scene/Camera/CameraManager.h"
#include "GameEngine/Scene/Camera/Camera.h"

namespace GameEngine {
Scene::Scene(SceneDefinition definition, CameraManager& cameras) : cameras_(cameras), definition_(std::move(definition)) {
}

Scene::~Scene() {
   Finalize();
}

bool Scene::PrepareAssets(const nlohmann::json& data, AssetManager& assets) {
   try {
      if (!SceneWorld::IsValidSnapshot(data)) {
         return false;
      }

      const auto resolved = assets.GetParameterAssetStore().ResolveScene(data);
      return PreloadSceneTextures(*assets.GetTextureManager(), resolved);
   } catch (const std::exception& error) {
      Logger::Error(std::string("Scene asset preparation failed: ") + error.what());
      return false;
   }
}

bool Scene::Initialize(const nlohmann::json& data) {
   auto camera = std::make_unique<Camera>();
   camera->Initialize();
   camera->SetFarClip(10000.0f);
   world_.SetCameraBrain(cameras_.CreateUnit(std::move(camera)));
   initialized_ = true;
   return world_.LoadFromJson(data);
}

void Scene::Finalize() {
   if (!initialized_) return;
   world_.Clear();
   world_.SetCameraBrain(nullptr);
   cameras_.ClearUnits();
   initialized_ = false;
}
}
