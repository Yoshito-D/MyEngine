#include "Game.h"
#include "Utility/Logger.h"

#ifdef USE_IMGUI
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace {
constexpr int kEditorSessionStateFormatVersion = 1;
constexpr int kEditorSessionStateJsonIndentSize = 3;

std::filesystem::path GetEditorSessionStateFilePath() {
   return std::filesystem::path("resources") / "game" / "editor" / "session_state.json";
}

std::string LoadLastEditorSceneName(const SceneCatalog& sceneCatalog) {
   const std::filesystem::path filePath = GetEditorSessionStateFilePath();
   std::ifstream file(filePath);
   if (!file.is_open()) {
      return {};
   }

   // セッションファイルの破損は起動失敗にせず、カタログ既定シーンへフォールバックする。
   nlohmann::json sessionState;
   try {
      file >> sessionState;
   } catch (const nlohmann::json::exception& exception) {
      Logger::Warning(
         "Editor session state contains invalid JSON: " + std::string(exception.what()),
         Logger::LogChannel::Editor);
      return {};
   }

   if (!sessionState.is_object()) {
      return {};
   }

   const std::string sceneName = sessionState.value("lastSceneName", "");
   // 削除・改名済みのシーンで起動不能にならないよう、現在のカタログを正とする。
   return sceneCatalog.Contains(sceneName) ? sceneName : std::string();
}

void SaveLastEditorSceneName(const std::string& sceneName) {
   if (sceneName.empty()) {
      return;
   }

   // 初回起動でも保存できるよう、状態ファイルより先に親ディレクトリを用意する。
   const std::filesystem::path filePath = GetEditorSessionStateFilePath();
   std::error_code error;
   std::filesystem::create_directories(filePath.parent_path(), error);
   if (error) {
      Logger::Warning(
         "Editor session directory could not be created: " + error.message(),
         Logger::LogChannel::Editor);
      return;
   }

   std::ofstream file(filePath);
   if (!file.is_open()) {
      Logger::Warning(
         "Editor session state could not be saved: " + filePath.generic_string(),
         Logger::LogChannel::Editor);
      return;
   }

   const nlohmann::json sessionState = {
      { "version", kEditorSessionStateFormatVersion },
      { "lastSceneName", sceneName },
   };
   file << sessionState.dump(kEditorSessionStateJsonIndentSize);
}
}
#endif

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
   std::string initialSceneName = sceneCatalog_.GetInitialSceneName();
#ifdef USE_IMGUI
   if (const std::string lastSceneName = LoadLastEditorSceneName(sceneCatalog_); !lastSceneName.empty()) {
      initialSceneName = lastSceneName;
   }
#endif
   sceneManager_->ChangeScene(initialSceneName);

}

void Game::Update() {
   if (!sceneManager_) {
      return;
   }
#ifdef USE_IMGUI
   // エディタでは入力・時間を進めるランタイム更新と、選択表示などの編集更新を分離する。
   playModeController_->ProcessRequests(*sceneManager_);
   if (playModeController_->ShouldRunRuntimeUpdate()) {
      Framework::Update();
   }
   sceneManager_->EditorUpdate();
   if (playModeController_->ShouldRunRuntimeUpdate()) {
      GameEngine::EngineContext::AdvanceGameFrameNumber();
      sceneManager_->RuntimeUpdate();
   }
#else
   Framework::Update();
   GameEngine::EngineContext::AdvanceGameFrameNumber();
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
#ifdef USE_IMGUI
      // 次回の編集開始位置だけを保存し、ランタイム中のシーン状態そのものは持ち越さない。
      SaveLastEditorSceneName(sceneManager_->GetCurrentSceneName());
#endif
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
      // 描画中の参照を無効化しないよう、予約されたシーン遷移はフレーム末尾で確定する。
      sceneManager_->CheckSceneChange();
   }
}
