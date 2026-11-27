#include "GameProject/Game.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/UI/UISystem.h"
#include "GameEngine/Utility/Logger.h"
#include "GameProject/App/Component/UI/PauseMenuComponent.h"

void Game::Initialize() {
   // ランタイムの操作はキーボードとゲームパッドで受け付ける。
   InitializeEngine(false, {"Resources/game/config/input_actions.json", "Saved/input_actions.json"});

   // シーンの音声を開始する前に、前回保存したユーザー音量を復元する。
   App::PauseMenuComponent::LoadAudioSettings(GetAudioService());

   if (!sceneCatalog_.Load()) {
      Logger::Fail(sceneCatalog_.GetLastError(), Logger::LogChannel::Game);
   }
   sceneManager_ = CreateSceneManager(sceneCatalog_);
   GameEngine::EngineContext::SetSceneManager(sceneManager_.get());
   auto initialSceneName = sceneCatalog_.GetInitialSceneName();
#ifdef USE_IMGUI
   editor_.emplace(*sceneManager_, GetAudioService(), "Resources/game/editor/session_state.json");
   initialSceneName = editor_->ResolveInitialScene();
#endif
   sceneManager_->LoadInitialScene(initialSceneName);
   CheckSceneFailure();
}

void Game::CheckSceneFailure() {
   if (!sceneManager_ ||
       sceneManager_->GetLastResult().status != GameEngine::SceneLoadStatus::Fatal) {
      return;
   }
   Logger::Fail(sceneManager_->GetLastResult().message, Logger::LogChannel::Game);
}

void Game::Update() {
   if (!sceneManager_) return;

   if (auto* scene = sceneManager_->GetCurrentScene()) {
      scene->GetSceneWorld().SynchronizeParameters();
   }

   const float realDeltaTime = GetRealDeltaTime();

#ifdef USE_IMGUI
   // エディタでは入力・時間を進めるランタイム更新と、選択表示などの編集更新を分離する。
   const auto timing = editor_->BeginFrame(realDeltaTime);
   const bool runRuntime = timing.runRuntime;
   const bool editorPaused = timing.audioPaused;
   const float uiDeltaTime = timing.uiSeconds;
   float gameDeltaTime = timing.gameSeconds;
#else
   const bool runRuntime = true;
   const bool editorPaused = false;
   const float uiDeltaTime = realDeltaTime;
   float gameDeltaTime = realDeltaTime;
#endif

   // 停止要求の処理でシーンが置き換わるため、処理後のシーンを使う。
   auto* scene = sceneManager_->GetCurrentScene();

   if (!scene) {
      CheckSceneFailure();
      return;
   }

   auto& world = scene->GetSceneWorld();
   auto& ui = world.GetUI();
   GameEngine::EngineContext::SetGameDeltaTime(gameDeltaTime);
   UpdateUIFrame(ui, runRuntime, uiDeltaTime);

   if (ui.PausesGameplay()) {
      gameDeltaTime = 0.0f;
      GameEngine::EngineContext::SetGameDeltaTime(gameDeltaTime);
   }

   if (runRuntime) {
      Framework::Update();
   }

   sceneManager_->UpdateTransitionFade();

#ifdef USE_IMGUI
   editor_->Update();
#endif

   if (runRuntime) {
      world.Update(gameDeltaTime, uiDeltaTime);
   }

   auto& audio = GetAudioService();
   audio.SetGamePaused(editorPaused || ui.PausesGameplay(), editorPaused);
   audio.Update(realDeltaTime);
}

void Game::Draw() {
#ifdef USE_IMGUI
   if (editor_) {
      editor_->Draw();
   }
#endif
}

void Game::Finalize() {
   if (sceneManager_) {
#ifdef USE_IMGUI
      if (editor_) {
         editor_->SaveCurrentScene();
      }
#endif
      sceneManager_->Finalize();
   }
#ifdef USE_IMGUI
   editor_.reset();
#endif
   GameEngine::EngineContext::SetSceneManager(nullptr);
   sceneManager_.reset();
   Framework::Finalize();
}

void Game::EndFrame() {
   auto* scene = sceneManager_ ? sceneManager_->GetCurrentScene() : nullptr;
   auto* world = scene ? &scene->GetSceneWorld() : nullptr;
   RenderFrame(world);

   if (world) {
      world->FinishFrame();
   }

   if (sceneManager_) {
      // 描画中の参照を無効化しないよう、予約されたシーン遷移はフレーム末尾で確定する。
      sceneManager_->CommitPendingSceneChange();
      CheckSceneFailure();
   }
}
