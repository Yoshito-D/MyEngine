#include "GameProject/Game.h"
#include "GameProject/App/Component/UI/PauseMenuComponent.h"
#include "GameEngine/UI/UISystem.h"
#include <cstdint>
#include "GameEngine/Utility/Logger.h"
#include "GameProject/App/Component/Rendering/PlayerShadowComponent.h"
#include "GameProject/App/Component/Race/RaceGoalDirectionWorldComponent.h"
#include "GameProject/App/Component/Race/UI/RaceGoalDirectionHUDComponent.h"
#include "GameProject/App/Component/Race/UI/RaceMinimapHUDComponent.h"
#include "GameEngine/Object/Model/Model.h"

#ifdef USE_IMGUI
#include "GameEngine/Utility/JsonFile.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#endif

namespace {
GameEngine::InputActionConfiguration GetGameInputConfiguration() {
   return {"Resources/game/config/input_actions.json", "Saved/input_actions.json"};
}

#ifdef USE_IMGUI
constexpr std::int32_t kEditorSessionStateFormatVersion = 1;
constexpr std::int32_t kEditorSessionStateJsonIndentSize = 3;

std::filesystem::path GetEditorSessionStateFilePath() {
   return std::filesystem::path("resources") / "game" / "editor" / "session_state.json";
}

std::string LoadLastEditorSceneName(const GameEngine::SceneCatalog& sceneCatalog) {
   const std::filesystem::path filePath = GetEditorSessionStateFilePath();
   std::ifstream file(filePath);

   if (!file.is_open()) {
      if (std::filesystem::exists(filePath)) {
         Logger::Fail("Cannot open editor session state: " + filePath.generic_string(), Logger::LogChannel::Editor);
      }

      return {};
   }
   nlohmann::json sessionState;
   try {
      file >> sessionState;
   } catch (const nlohmann::json::exception& exception) {
      Logger::Fail("Invalid editor session JSON: " + filePath.generic_string() + ": " + exception.what(),
         Logger::LogChannel::Editor);
   }

   if (!sessionState.is_object() || !sessionState.contains("version") ||
       !sessionState.at("version").is_number_integer() ||
       sessionState.at("version") != kEditorSessionStateFormatVersion) {
      Logger::Fail("Invalid editor session state version: " + filePath.generic_string(), Logger::LogChannel::Editor);
   }

   if (!sessionState.contains("lastSceneName") ||
       !sessionState.at("lastSceneName").is_string()) {
      Logger::Fail("Missing or invalid lastSceneName: " + filePath.generic_string(), Logger::LogChannel::Editor);
   }

   const auto& sceneName = sessionState.at("lastSceneName").get_ref<const std::string&>();

   if (!sceneCatalog.Contains(sceneName)) {
      Logger::Fail("Unknown editor session scene: " + sceneName + " in " + filePath.generic_string(),
         Logger::LogChannel::Editor);
   }

   return sceneName;
}

void SaveLastEditorSceneName(const std::string& sceneName) {
   if (sceneName.empty()) return;

   const nlohmann::json sessionState = {
      {"version", kEditorSessionStateFormatVersion},
      {"lastSceneName", sceneName},
   };
   const auto filePath = GetEditorSessionStateFilePath();

   if (!GameEngine::SaveJsonFileAtomically(filePath, sessionState, kEditorSessionStateJsonIndentSize)) {
      Logger::Warning(
         "Editor session state could not be saved: " + filePath.generic_string(), Logger::LogChannel::Editor);
   }
}
#endif
}

void Game::Initialize() {
   // ランタイムの操作はキーボードとゲームパッドで受け付ける。
   InitializeEngine(false, GetGameInputConfiguration());

   // シーンの音声を開始する前に、前回保存したユーザー音量を復元する。
   App::PauseMenuComponent::LoadAudioSettings(GetAudioService());

   if (!sceneCatalog_.Load()) {
      Logger::Fail(sceneCatalog_.GetLastError(), Logger::LogChannel::Game);
   }
   sceneManager_ = CreateSceneManager(sceneCatalog_);
   GameEngine::EngineContext::SetSceneManager(sceneManager_.get());
#ifdef USE_IMGUI
   playModeController_ = std::make_unique<GameEngine::PlayModeController>(GetAudioService());
   GameEngine::EngineContext::SetPlayModeController(playModeController_.get());
   editorSession_ = std::make_unique<GameEngine::EditorSceneSession>();
   GameEngine::EngineContext::SetEditorSceneSession(editorSession_.get());
   sceneManager_->SetLifecycleCallbacks(
      [this]() {
         editorSession_->Detach();
         playModeController_->StopForSceneInitialization();
      },
      [this](GameEngine::Scene& scene, const GameEngine::SceneSnapshot& snapshot) {
         editorSession_->Attach(scene, snapshot.data, false);
      },
      [this](const GameEngine::Scene& scene) {
         auto* context = editorSession_->GetContext();
         const bool fromPlay = playModeController_->HasRecoverySnapshot();
         const bool wasDirty = fromPlay ? playModeController_->GetPlayStartWasDirty() : context && context->IsDirty();
         auto snapshot = fromPlay ? playModeController_->GetRecoverySnapshot()
                                  : GameEngine::SceneSnapshot{scene.GetDefinition(),
                                       context ? context->SerializeToJson() : scene.GetSceneWorld().SerializeToJson()};

         return GameEngine::SceneRecovery{std::move(snapshot),
            [this, fromPlay, wasDirty](GameEngine::Scene& restored, const GameEngine::SceneSnapshot& recovery) {
               if (fromPlay) {
                  editorSession_->SetRestoreEditingStateOnAttach(true);
               }

               editorSession_->Attach(restored, recovery.data, !fromPlay);
               playModeController_->StopAfterLoadFailure();

               if (auto* restoredContext = editorSession_->GetContext()) {
                  if (wasDirty) {
                     restoredContext->MarkDirty();
                  } else {
                     restoredContext->ClearDirty();
                  }
               }
            }};
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
   playModeController_->ProcessRequests(*sceneManager_, realDeltaTime);
   const bool runRuntime = playModeController_->ShouldRunRuntimeUpdate();
   const bool editorPaused = !playModeController_->IsPlaying();
   const float uiDeltaTime = playModeController_->IsPaused() ? playModeController_->GetStepDeltaTime() : realDeltaTime;
   float gameDeltaTime = playModeController_->GetGameDeltaTime();
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
   editorSession_->Update();
#endif

   if (runRuntime) {
      world.Update(gameDeltaTime, uiDeltaTime);
   }

   auto& audio = GetAudioService();
   audio.SetGamePaused(editorPaused || ui.PausesGameplay(), editorPaused);
   audio.Update(realDeltaTime);
}

void Game::Draw() {
   // このフレームに対象が見つからなければ、影を描かない。
   GameEngine::EngineContext::ClearPlayerShadowFrameData();

   auto* scene = sceneManager_ ? sceneManager_->GetCurrentScene() : nullptr;

   if (!scene) return;

#ifdef USE_IMGUI
   editorSession_->Draw();
#endif

   auto& world = scene->GetSceneWorld();
   auto* camera = GameEngine::EngineContext::GetActiveCamera();
#ifdef USE_IMGUI
   if (!GameEngine::EngineContext::IsPlayModeEdit())
#endif
   {
      auto* map = world.FindObjectById(App::RaceMinimapHUDComponent::kObjectId);
      auto* minimap = map ? map->GetComponent<App::RaceMinimapHUDComponent>() : nullptr;
      auto* arrow = world.FindObjectById(App::RaceGoalDirectionWorldComponent::kObjectId);
      auto* worldArrow = arrow ? arrow->GetComponent<App::RaceGoalDirectionWorldComponent>() : nullptr;
      App::RaceGoalDirectionHUDComponent* hudArrow = nullptr;

      // 車体上の表示が有効なら同じモデルをHUDへ送らない。無効に切り替えると距離表示の横に戻せる。
      if (worldArrow && worldArrow->IsEnabled()) {
         worldArrow->Present(camera);
      } else if (arrow) {
         hudArrow = arrow->GetComponent<App::RaceGoalDirectionHUDComponent>();
      }

      if (minimap) {
         minimap->Submit(hudArrow);
      } else if (hudArrow) {
         hudArrow->Hide();
      }
   }

   if (!camera) return;

   // 影描画コンポーネントを持つプレイヤーが1体の構成。
   for (auto* model : world.GetObjectStore().GetModels()) {
      if (!model) {
         continue;
      }

      auto* vehicle = model->GetComponent<App::PlayerShadowComponent>();

      // レース中の操作ロックで無効になっていても、停止中の車体の影は描画する。
      if (!vehicle || !vehicle->IsEnabled()) {
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
