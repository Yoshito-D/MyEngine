#include "Game.h"
#include "Utility/Logger.h"

void Game::Initialize() {
   Framework::Initialize();
   Logger::GameInfo("Game initialized.");

   // シーンマネージャーの初期化
   factory_ = std::make_unique<MySceneFactory>();
   sceneManager_ = std::make_unique<GameEngine::SceneManager>(factory_.get());

#ifdef USE_IMGUI
   playModeController_ = std::make_unique<GameEngine::PlayModeController>();
   GameEngine::EngineContext::SetPlayModeController(playModeController_.get());
#endif

   // 最初のシーンを設定
#ifdef NDEBUG
	 sceneManager_->ChangeScene("GameTest");
#else
	 sceneManager_->ChangeScene("GameTest");
#endif

}

void Game::Update() {
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
   sceneManager_->Draw();
}

void Game::Finalize() {
   sceneManager_->Finalize();
#ifdef USE_IMGUI
   GameEngine::EngineContext::SetPlayModeController(nullptr);
   playModeController_.reset();
#endif
   Framework::Finalize();
}

void Game::EndFrame() {
   Framework::EndFrame();
   sceneManager_->CheckSceneChange();
}
