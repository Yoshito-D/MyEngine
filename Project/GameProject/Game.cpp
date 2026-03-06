#include "Game.h"
#include "Utility/Logger.h"

void Game::Initialize() {
   Framework::Initialize();

   // シーンマネージャーの初期化
   factory_ = std::make_unique<MySceneFactory>();
   sceneManager_ = std::make_unique<GameEngine::SceneManager>(factory_.get());

   // 最初のシーンを設定
   sceneManager_->ChangeScene("Test");
}

void Game::Update() {
   sceneManager_->Update();
}

void Game::Draw() {
   sceneManager_->Draw();
}

void Game::Finalize() {
   sceneManager_->Finalize();
   Framework::Finalize();
}

void Game::EndFrame() {
   Framework::EndFrame();
   sceneManager_->CheckSceneChange();
}
