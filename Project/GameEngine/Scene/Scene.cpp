#include "GameEngine/pch.h"
#include "GameEngine/Scene/Scene.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Scene/Camera/Camera.h"

namespace GameEngine {
Scene::Scene(SceneDefinition definition) : definition_(std::move(definition)) {
}

Scene::~Scene() {
   Finalize();
}

bool Scene::Initialize(const nlohmann::json& data) {
   // 初回描画やイベント発火で読込が発生しないよう、保存状態の参照を先に準備する。
   if (!SceneWorld::IsValidSnapshot(data) || !EngineContext::PreloadSceneTextures(data)) {
      return false;
   }

   auto camera = std::make_unique<Camera>();
   camera->Initialize();
   camera->SetFarClip(10000.0f);
   world_.SetCameraBrain(EngineContext::CreateCameraUnit(std::move(camera)));
   initialized_ = true;
   return world_.LoadFromJson(data);
}

void Scene::Finalize() {
   if (!initialized_) return;
   world_.Clear();
   world_.SetCameraBrain(nullptr);
   EngineContext::ClearCameraUnits();
   initialized_ = false;
}

void Scene::RuntimeUpdate(float deltaTime) {
   world_.Update(deltaTime);
}
}
