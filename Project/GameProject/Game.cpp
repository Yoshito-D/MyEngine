#include "Game.h"
#include "Utility/Logger.h"

void Game::Initialize() {
   Framework::Initialize();
   Logger::GameInfo("Game initialized.");

   // シーンマネージャーの初期化
   if (!sceneCatalog_.Load()) {
      Logger::Critical("Scene catalog initialization failed.");
      return;
   }
   factory_ = std::make_unique<MySceneFactory>(sceneCatalog_);
   sceneManager_ = std::make_unique<GameEngine::SceneManager>(factory_.get());

#ifdef USE_IMGUI
   playModeController_ = std::make_unique<GameEngine::PlayModeController>();
   GameEngine::EngineContext::SetPlayModeController(playModeController_.get());
#endif

   // 最初のシーンを設定
   sceneManager_->ChangeScene(sceneCatalog_.GetInitialSceneName());

}

void Game::Update() {
   if (!sceneManager_) {
      return;
   }
#ifdef USE_IMGUI
   playModeController_->ProcessRequests(*sceneManager_);
   if (playModeController_->ShouldRunRuntimeUpdate()) {
      Framework::Update();
   }
   sceneManager_->EditorUpdate();
   if (playModeController_->ShouldRunRuntimeUpdate()) {
      sceneManager_->RuntimeUpdate();
   }
#else
   Framework::Update();
   sceneManager_->Update();
#endif
}

void Game::Draw() {
   if (sceneManager_) {
      sceneManager_->Draw();
   }
}

void Game::Finalize() {
   if (sceneManager_) {
      sceneManager_->Finalize();
   }
#ifdef USE_IMGUI
   GameEngine::EngineContext::SetPlayModeController(nullptr);
   playModeController_.reset();
#endif
   Framework::Finalize();
}

void Game::EndFrame() {
   Framework::EndFrame();
   if (sceneManager_) {
      sceneManager_->CheckSceneChange();
   }
}
