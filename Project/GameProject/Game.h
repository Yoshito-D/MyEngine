#pragma once
#include "Framework.h"
#include "EngineContext.h"
#include "SceneManager.h"
#include "App/Scene/MySceneFactory.h"

class Game : public GameEngine::Framework {
public:
   void Initialize() override;
   void Update() override;
   void Draw() override;
   void Finalize() override;
   void EndFrame() override;
private:
   std::unique_ptr<MySceneFactory> factory_ = nullptr;
   std::unique_ptr<GameEngine::SceneManager> sceneManager_ = nullptr;
};