#include "GameProject/Game.h"
#include <cstdint>
#include "GameEngine/Utility/Logger.h"
#include "GameProject/App/Component/Vehicle/VehicleController.h"
#include "GameEngine/Object/Model/Model.h"

#ifdef USE_IMGUI
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace {
constexpr std::int32_t kEditorSessionStateFormatVersion = 1;
constexpr std::int32_t kEditorSessionStateJsonIndentSize = 3;

std::filesystem::path GetEditorSessionStateFilePath() {
   return std::filesystem::path("resources") / "game" / "editor" / "session_state.json";
}

std::string LoadLastEditorSceneName(const GameEngine::SceneCatalog& sceneCatalog) {
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
         "Editor session state contains invalid JSON: " + std::string(exception.what()), Logger::LogChannel::Editor);
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
   if (sceneName.empty()) return;

   // 初回起動でも保存できるよう、状態ファイルより先に親ディレクトリを用意する。
   const std::filesystem::path filePath = GetEditorSessionStateFilePath();
   std::error_code error;
   std::filesystem::create_directories(filePath.parent_path(), error);

   if (error) {
      Logger::Warning("Editor session directory could not be created: " + error.message(), Logger::LogChannel::Editor);
      return;
   }

   std::ofstream file(filePath);

   if (!file.is_open()) {
      Logger::Warning(
         "Editor session state could not be saved: " + filePath.generic_string(), Logger::LogChannel::Editor);
      return;
   }

   const nlohmann::json sessionState = {
      {"version", kEditorSessionStateFormatVersion},
      {"lastSceneName", sceneName},
   };
   file << sessionState.dump(kEditorSessionStateJsonIndentSize);
}
}
#endif

void Game::Initialize() {
   Framework::Initialize();
   if (!sceneCatalog_.Load()) {
      Logger::Critical(sceneCatalog_.GetLastError());
      MessageBoxA(nullptr, sceneCatalog_.GetLastError().c_str(), "Scene catalog failed", MB_OK | MB_ICONERROR);
      PostQuitMessage(1);
      return;
   }
   sceneManager_ = std::make_unique<GameEngine::SceneManager>(sceneCatalog_);
   GameEngine::EngineContext::SetSceneManager(sceneManager_.get());
#ifdef USE_IMGUI
   playModeController_ = std::make_unique<GameEngine::PlayModeController>();
   GameEngine::EngineContext::SetPlayModeController(playModeController_.get());
   editorSession_ = std::make_unique<GameEngine::EditorSceneSession>();
   GameEngine::EngineContext::SetEditorSceneSession(editorSession_.get());
   sceneManager_->SetLifecycleCallbacks(
      [this]() {
         editorSession_->Detach();
         playModeController_->StopForSceneInitialization();
      },
      [this](const GameEngine::SceneSnapshot& snapshot, bool recovered) {
         if (recovered && recoveryFromPlay_) {
            editorSession_->RestoreEditingStateOnAttach();
         }
         editorSession_->Attach(*sceneManager_->GetCurrentScene(), snapshot.data, recovered && !recoveryFromPlay_);
         if (recovered) {
            playModeController_->StopAfterLoadFailure();
            if (auto* context = editorSession_->GetContext()) {
               if (recoveryWasDirty_) {
                  context->MarkDirty();
               } else {
                  context->ClearDirty();
               }
            }
         }
      },
      [this]() {
         recoveryFromPlay_ = playModeController_->HasRecoverySnapshot();
         if (recoveryFromPlay_) {
            recoveryWasDirty_ = playModeController_->GetPlayStartWasDirty();
            return playModeController_->GetRecoverySnapshot();
         }
         auto* context = editorSession_->GetContext();
         recoveryWasDirty_ = context && context->IsDirty();
         auto* scene = sceneManager_->GetCurrentScene();
         return GameEngine::SceneSnapshot{
            scene->GetDefinition(), context ? context->SerializeToJson() : scene->GetSceneWorld().SerializeToJson()};
      });
#endif
   std::string initialSceneName = sceneCatalog_.GetInitialSceneName();
#ifdef USE_IMGUI
   if (const auto last = LoadLastEditorSceneName(sceneCatalog_); !last.empty()) {
      initialSceneName = last;
   }
#endif
   sceneManager_->LoadInitialScene(initialSceneName);
   CheckSceneFailure();
}

void Game::CheckSceneFailure() {
   if (fatalSceneFailureReported_ || !sceneManager_ ||
       sceneManager_->GetLastResult().status != GameEngine::SceneLoadStatus::Fatal) {
      return;
   }
   fatalSceneFailureReported_ = true;
   MessageBoxA(nullptr, sceneManager_->GetLastResult().message.c_str(), "Scene loading failed", MB_OK | MB_ICONERROR);
   PostQuitMessage(1);
}

void Game::Update() {
   if (!sceneManager_) return;
#ifdef USE_IMGUI
   // エディタでは入力・時間を進めるランタイム更新と、選択表示などの編集更新を分離する。
   playModeController_->ProcessRequests(*sceneManager_);

   if (playModeController_->ShouldRunRuntimeUpdate()) {
      Framework::Update();
   }

   sceneManager_->EditorUpdate();
   editorSession_->Update();

   if (playModeController_->ShouldRunRuntimeUpdate()) {
      GameEngine::EngineContext::AdvanceGameFrameNumber();
      sceneManager_->RuntimeUpdate();
   }
#else
   Framework::Update();
   GameEngine::EngineContext::AdvanceGameFrameNumber();
   sceneManager_->EditorUpdate();
   sceneManager_->RuntimeUpdate();
#endif
   CheckSceneFailure();
   if (auto* audio = GameEngine::EngineContext::GetAudio()) {
      audio->Update(GameEngine::EngineContext::GetUnscaledDeltaTime());
   }
}

void Game::Draw() {
   // このフレームに対象が見つからなければ、影を描かない。
   GameEngine::EngineContext::ClearPlayerShadowFrameData();

   if (!sceneManager_) return;

#ifdef USE_IMGUI
   editorSession_->Draw();
#endif

   auto* camera = GameEngine::EngineContext::GetActiveCamera();

   if (!camera) return;

   // VehicleControllerを持つプレイヤーが1体の構成。
   for (auto* model : sceneManager_->GetCurrentScene()->GetSceneWorld().GetObjectStore().GetModels()) {
      if (!model) {
         continue;
      }

      auto* vehicle = model->GetComponent<App::VehicleController>();

      // レース中の操作ロックで無効になっていても、停止中の車体の影は描画する。
      if (!vehicle) {
         continue;
      }

      vehicle->SubmitPlayerShadow(camera);
      break;
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
   GameEngine::EngineContext::SetEditorSceneSession(nullptr);
   editorSession_.reset();
   GameEngine::EngineContext::SetPlayModeController(nullptr);
   playModeController_.reset();
#endif
   GameEngine::EngineContext::SetSceneManager(nullptr);
   sceneManager_.reset();
   Framework::Finalize();
}

void Game::EndFrame() {
   Framework::EndFrame();

   if (sceneManager_) {
      // 描画中の参照を無効化しないよう、予約されたシーン遷移はフレーム末尾で確定する。
      if (auto* scene = sceneManager_->GetCurrentScene()) {
         scene->GetSceneWorld().FinishFrame();
      }
      sceneManager_->CheckSceneChange();
      CheckSceneFailure();
   }
}
