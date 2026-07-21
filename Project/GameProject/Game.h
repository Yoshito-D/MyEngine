#pragma once
#include "Framework.h"
#include "EngineContext.h"
#include "SceneManager.h"
#include "App/Scene/MySceneFactory.h"
#include "App/Scene/SceneCatalog.h"
#ifdef USE_IMGUI
#include "PlayModeController.h"
#endif

class Game : public GameEngine::Framework {
public:
   void Initialize() override;
   void Update() override;
   void Draw() override;
   void Finalize() override;
   void EndFrame() override;
private:
	SceneCatalog sceneCatalog_;
   std::unique_ptr<MySceneFactory> factory_ = nullptr;
   std::unique_ptr<GameEngine::SceneManager> sceneManager_ = nullptr;
#ifdef USE_IMGUI
   std::unique_ptr<GameEngine::PlayModeController> playModeController_ = nullptr;
#endif
};
