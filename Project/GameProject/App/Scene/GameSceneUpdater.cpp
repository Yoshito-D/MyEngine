#include "GameSceneUpdater.h"
#include "Framework/EngineContext.h"
#include "LevelEditor.h"
#include "../GameObject/Player/Player.h"
#include "../GameObject/Planet/Planet.h"
#include "../GameObject/Star/Star.h"
#include "../GameObject/Rabbit/Rabbit.h"
#include "../GameObject/Box/Box.h"
#include "../Collider/CollisionManager.h"
#include "../Camera/TPSCameraController.h"
#include "../Camera/CameraSequencer.h"
#include "../Camera/CameraSequenceEditor.h"
#include "../Camera/OrbitalCameraController.h"
#include "../UI/TimerDisplay.h"
#include "Utility/StateMachine.h"
#include "Utility/Math/Transform.h"
#include "Effect/ParticleSystem.h"
#include "Effect/ParticleSystemEdit.h"
#include "Scene/Camera/DebugCamera.h"
#include "Scene/Camera/Camera.h"
#include "Scene/SceneFade.h"
#include "Object/Model/Model.h"

#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui.h"
#endif

using namespace GameEngine;

void GameSceneUpdater::UpdateDebugFeatures(
   LevelEditor* levelEditor,
   bool& autoReload,
   std::string& currentLevelFile,
   bool& isDebugCameraActive,
   Camera* mainCamera,
   GameEngine::Transform& mainCameraPrevTransform,
   std::function<void()> loadLevelCallback,
   std::function<void()> exportLevelCallback
) {
#ifdef USE_IMGUI
   // レベルエディターの更新
   if (levelEditor) {
      levelEditor->ShowEditor();
      
      // ホットリロード（ファイル監視）
      if (autoReload) {
         levelEditor->Update(currentLevelFile);
         // エディターのデータが変更された場合、レベルを再ロード
         if (levelEditor->IsDataChanged()) {
            if (loadLevelCallback) {
               loadLevelCallback();
            }
            levelEditor->ResetDataChangedFlag();
         }
      }
      
      // エディターの手動操作
      ImGui::Begin("Level Editor Controls");
      
      ImGui::Checkbox("Auto Reload", &autoReload);
      
      char filePathBuffer[256];
      strcpy_s(filePathBuffer, currentLevelFile.c_str());
      if (ImGui::InputText("Level File", filePathBuffer, sizeof(filePathBuffer))) {
         currentLevelFile = filePathBuffer;
      }
      
      if (ImGui::Button("Load Level from Editor")) {
         if (loadLevelCallback) {
            loadLevelCallback();
         }
      }
      
      if (ImGui::Button("Export Current Level to Editor")) {
         if (exportLevelCallback) {
            exportLevelCallback();
         }
      }
      
      if (ImGui::Button("Reload Level from File")) {
         if (levelEditor->LoadFromJson(currentLevelFile)) {
            if (loadLevelCallback) {
               loadLevelCallback();
            }
         }
      }
      
      ImGui::End();
   }

   // デバッグカメラの切り替え
   if (EngineContext::IsKeyTriggered(KeyCode::F1)) {
      isDebugCameraActive = !isDebugCameraActive;
      if (isDebugCameraActive) {
         mainCameraPrevTransform = mainCamera->GetTransform();
      } else {
         mainCamera->SetTransform(mainCameraPrevTransform);
      }
   }
#endif
}

void GameSceneUpdater::UpdateCamera(
   bool isDebugCameraActive,
   bool isOpeningSequencePlaying,
   bool isOrbitalCameraPlaying,
   DebugCamera* debugCamera,
   TPSCameraController* tpsCameraController
) {
#ifdef USE_IMGUI
   // デバッグカメラが有効な場合は最優先で処理
   if (isDebugCameraActive && debugCamera) {
      if (EngineContext::GetIsSceneHovered()) {
         debugCamera->Update();
         debugCamera->ApplyCameraTransform();
      } else {
         debugCamera->ApplyCameraTransform();
      }
      return; // デバッグカメラ使用中は他のカメラ処理をスキップ
   }
#endif

   // デバッグカメラが無効の場合の通常処理
   if (isOpeningSequencePlaying || isOrbitalCameraPlaying) {
      // オープニングシーケンス中または軌道カメラ中はそれぞれが制御
      // カメラの更新は既にUpdateOpening()で行われている
   } else if (tpsCameraController) {
      // 通常時はTPSカメラが制御
      tpsCameraController->Update();
   }
}

void GameSceneUpdater::UpdateCameraSequence(
   CameraSequenceEditor* cameraEditor,
   CameraSequencer* openingSequencer,
   OrbitalCameraController* orbitalCamera,
   Camera* mainCamera,
   std::string& openingSequenceFile,
   bool isOpeningSequencePlaying,
   bool isOrbitalCameraPlaying,
   bool isWaitingForOrbitalFadeOut,
   float orbitalCameraEndTimer,
   StateMachine* stateMachine,
   GameEngine::SceneFade* sceneFade,
   std::function<void()> startOrbitalCallback
) {
   // 未使用パラメータの警告を抑制
   (void)startOrbitalCallback;
   
#ifdef USE_IMGUI
   // カメラシーケンスエディターの表示
   if (cameraEditor) {
      cameraEditor->ShowEditorWindow(mainCamera);
      
      // エディターウィンドウ
      ImGui::Begin("Camera Sequence Controls");
      
      ImGui::Text("Opening Camera Sequence");
      ImGui::Separator();
      
      // ファイルパス
      char filePathBuffer[256];
      strcpy_s(filePathBuffer, openingSequenceFile.c_str());
      if (ImGui::InputText("Sequence File", filePathBuffer, sizeof(filePathBuffer))) {
         openingSequenceFile = filePathBuffer;
      }
      
      // リロードボタン
      if (ImGui::Button("Reload Sequence")) {
         if (cameraEditor->LoadFromFile(openingSequenceFile)) {
            if (openingSequencer) {
               cameraEditor->ApplyToSequencer(openingSequencer);
            }
            ImGui::Text("Reloaded successfully!");
         } else {
            ImGui::Text("Failed to reload!");
         }
      }
      
      ImGui::SameLine();
      if (ImGui::Button("Save Sequence")) {
         if (cameraEditor->SaveToFile(openingSequenceFile)) {
            ImGui::Text("Saved successfully!");
         } else {
            ImGui::Text("Failed to save!");
         }
      }
      
      ImGui::Separator();
      
      // 再生制御
      if (openingSequencer) {
         if (ImGui::Button("Play Opening")) {
            cameraEditor->ApplyToSequencer(openingSequencer);
            
            // 軌道カメラをリセット
            if (orbitalCamera) {
               orbitalCamera->Stop();
            }
            
            // ステートをOpeningに変更
            if (stateMachine) {
               stateMachine->RequestState("Opening", 100);
            }
            
            // オープニングシーケンスを開始
            openingSequencer->Play(false);
         }
         ImGui::SameLine();
         if (ImGui::Button("Stop")) {
            openingSequencer->Stop();
            
            // 軌道カメラも停止
            if (orbitalCamera) {
               orbitalCamera->Stop();
            }

            // ステートをPlayingに戻す
            if (stateMachine) {
               stateMachine->RequestState("Playing", 100);
            }
         }
         
         if (isOpeningSequencePlaying && openingSequencer->IsPlaying()) {
            ImGui::Text("Playing Keyframes: %.1f/%.1f sec",
                       openingSequencer->GetElapsedTime(),
                       openingSequencer->GetTotalDuration());
            ImGui::Text("Keyframe: %zu/%zu",
                       openingSequencer->GetCurrentKeyframeIndex() + 1,
                       openingSequencer->GetKeyframeCount());
         }
         
         if (isOrbitalCameraPlaying && orbitalCamera && orbitalCamera->IsMoving()) {
            ImGui::Text("Orbital Camera: %.1f%%", 
                       orbitalCamera->GetProgress() * 100.0f);
         }
      }
      
      ImGui::Separator();
      ImGui::Text("Status: %s", 
         isOrbitalCameraPlaying ? "Orbital Camera" :
         isOpeningSequencePlaying ? "Keyframe Playing" : "Stopped");
      

      // 現在のステート表示
      if (stateMachine) {
         ImGui::Text("Current State: %s", stateMachine->GetCurrentState().c_str());
      }
      
      // フェード状態表示
      if (isWaitingForOrbitalFadeOut) {
         ImGui::Text("Waiting for fade out: %.2f/1.5", orbitalCameraEndTimer);
      }
      if (sceneFade) {
         const char* fadeStateStr = "None";
         auto fadeState = sceneFade->GetFadeState();
         if (fadeState == GameEngine::SceneFade::FadeState::FadeIn) fadeStateStr = "FadeIn";
         else if (fadeState == GameEngine::SceneFade::FadeState::FadeOut) fadeStateStr = "FadeOut";
         ImGui::Text("Fade State: %s", fadeStateStr);
         ImGui::Text("Fade Out Completed: %s", sceneFade->IsFadeOutCompleted() ? "YES" : "NO");
      }
      if (openingSequencer && openingSequencer->GetSceneFade()) {
         auto seqFade = openingSequencer->GetSceneFade();
         const char* seqFadeStateStr = "None";
         auto seqFadeState = seqFade->GetFadeState();
         if (seqFadeState == GameEngine::SceneFade::FadeState::FadeIn) seqFadeStateStr = "FadeIn";
         else if (seqFadeState == GameEngine::SceneFade::FadeState::FadeOut) seqFadeStateStr = "FadeOut";
         ImGui::Text("Sequencer Fade State: %s", seqFadeStateStr);
      }
      
      ImGui::End();
   }
#endif
}

void GameSceneUpdater::UpdateGameObjects(
   float dt,
   std::vector<std::unique_ptr<Planet>>& planets,
   Player* player,
   Star* star,
   std::vector<std::unique_ptr<Rabbit>>& rabbits,
   std::vector<std::unique_ptr<Box>>& boxes,
   bool isOpeningSequencePlaying,
   bool isOrbitalCameraPlaying
) {
   // プレイヤーの更新を先に実行（オープニング中は停止）
   // これにより、プレイヤーは惑星が動く前の位置を基準に処理できる
   if (player && !isOpeningSequencePlaying && !isOrbitalCameraPlaying) {
      player->Update(dt);
   }

   // 全惑星の更新
   // プレイヤーの更新後に惑星を動かすことで、次フレームでプレイヤーが
   // 新しい惑星位置に追従できる
   for (auto& planet : planets) {
      if (planet) {
         planet->Update(dt);
      }
   }

   if (star) {
      star->Update(dt);
   }

   // 全ラビットの更新
   for (auto& rabbit : rabbits) {
      if (rabbit) {
         rabbit->Update(dt);
      }
   }
   
   // 全ボックスの更新
   for (auto& box : boxes) {
      if (box) {
         box->Update(dt);
      }
   }
}

void GameSceneUpdater::UpdateCollisions(CollisionManager* collisionManager) {
   if (collisionManager) {
      collisionManager->CheckAllCollisions();
   }
}

void GameSceneUpdater::ProcessRabbitRemoval(
   std::vector<Rabbit*>& rabbitsToRemove,
   std::vector<std::unique_ptr<Rabbit>>& rabbits,
   std::vector<std::unique_ptr<Model>>& rabbitModels,
   CollisionManager* collisionManager,
   std::vector<std::unique_ptr<Planet>>& planets,
   Player* player,
   Star* star,
   bool isStarActive,
   const std::vector<std::unique_ptr<class Box>>& boxes
) {
   // ラビットの削除処理
   if (!rabbitsToRemove.empty()) {
      for (auto* rabbitToRemove : rabbitsToRemove) {
         auto it = std::find_if(rabbits.begin(), rabbits.end(),
            [rabbitToRemove](const std::unique_ptr<Rabbit>& rabbit) {
               return rabbit.get() == rabbitToRemove;
            });

         if (it != rabbits.end()) {
            size_t index = std::distance(rabbits.begin(), it);

            // 範囲チェック
            if (index >= rabbitModels.size()) {
               continue;
            }
            
            // ラビットのポイントライトを削除
            std::string lightName = "RabbitPointLight_" + std::to_string(reinterpret_cast<uintptr_t>(rabbitToRemove));
            EngineContext::RemovePointLight(lightName);

            // モデルとゲームオブジェクトを両方削除
            rabbits.erase(it);
            rabbitModels.erase(rabbitModels.begin() + index);
         }
      }
      rabbitsToRemove.clear();

      // コリジョナーリストを再構築
      if (collisionManager) {
         collisionManager->Clear();

         // 惑星のコリジョンを再登録
         for (auto& planet : planets) {
            if (planet && planet->GetCollider()) {
               collisionManager->RegisterCollider(planet->GetCollider());
            }
         }

         // プレイヤーのコリジョンを再登録
         if (player && player->GetCollider()) {
            collisionManager->RegisterCollider(player->GetCollider());
         }
         
         // プレイヤーのスピンコライダーも再登録
         if (player && player->GetSpinCollider()) {
            collisionManager->RegisterCollider(player->GetSpinCollider());
         }

         // スターのコリジョンを再登録（有効な場合のみ）
         if (isStarActive && star && star->GetCollider()) {
            collisionManager->RegisterCollider(star->GetCollider());
         }

         // 残っているラビットのコリジョンを再登録
         for (auto& rabbit : rabbits) {
            if (rabbit && rabbit->GetCollider()) {
               collisionManager->RegisterCollider(rabbit->GetCollider());
            }
         }
         
         // ボックスのコリジョンを再登録
         for (const auto& box : boxes) {
            if (box && box->GetCollider()) {
               collisionManager->RegisterCollider(box->GetCollider());
            }
         }
      }
   }
}

void GameSceneUpdater::UpdateParticles(
   float dt,
   ParticleSystem* particleSmoke,
   ParticleSystem* particleStar,
   ParticleSystem* particleDust,
   ParticleSystem* particleStarCollection,
   ParticleSystem* particleRabbitCapture
) {
#ifdef USE_IMGUI
   if (particleSmoke) {
      ParticleSystemEdit::Edit(particleSmoke, "Smoke");
   }

   if (particleStar) {
      ParticleSystemEdit::Edit(particleStar, "Star");
   }

   if (particleDust) {
      ParticleSystemEdit::Edit(particleDust, "Dust");
   }
   
   if (particleStarCollection) {
      ParticleSystemEdit::Edit(particleStarCollection, "Star Collection");
   }
   
   if (particleRabbitCapture) {
      ParticleSystemEdit::Edit(particleRabbitCapture, "Rabbit Capture");
   }
#endif

   if (particleSmoke) {
      particleSmoke->Update(dt);
   }

   if (particleStar) {
      particleStar->Update(dt);
   }

   if (particleDust) {
      particleDust->Update(dt);
   }
   
   if (particleStarCollection) {
      particleStarCollection->Update(dt);
   }
   
   if (particleRabbitCapture) {
      particleRabbitCapture->Update(dt);
   }
}

void GameSceneUpdater::UpdateDebugUI(
   StateMachine* stateMachine,
   float stateTimer,
   int rabbitsCollected,
   int rabbitsRequired,
   bool isStarActive,
   Camera* mainCamera,
   Player* player,
   const std::vector<std::unique_ptr<Planet>>& planets,
   Star* star,
   const std::vector<std::unique_ptr<Rabbit>>& rabbits,
   const std::vector<std::unique_ptr<class Box>>& boxes
) {
#ifdef USE_IMGUI
   EngineContext::DebugDrawLights();

   // デバッグ情報表示
   ImGui::Begin("Game Scene Debug");

   // ステート情報
   if (stateMachine) {
      ImGui::Text("Current State: %s", stateMachine->GetCurrentState().c_str());
      ImGui::Text("State Timer: %.2f", stateTimer);
   }

   ImGui::Separator();
   ImGui::Text("操作方法");
   ImGui::Text("WASD: 移動");
   ImGui::Text("Space: ジャンプ");
   ImGui::Text("X: スピン");
   ImGui::Text("方向キー: カメラ回転");

   ImGui::Separator();
   ImGui::Text("ゲーム進行");
   ImGui::Text("捕まえたラビット: %d / %d", rabbitsCollected, rabbitsRequired);
   ImGui::Text("スター有効: %s", isStarActive ? "YES" : "NO");

   // カメラ情報を表示
   if (ImGui::CollapsingHeader("Camera Info")) {
      if (mainCamera) {
         Vector3 cameraPos = mainCamera->GetTransform().translation;
         Vector3 cameraRot = mainCamera->GetTransform().rotation;
         ImGui::Text("Position: (%.2f, %.2f, %.2f)", cameraPos.x, cameraPos.y, cameraPos.z);
         ImGui::Text("Rotation: (%.2f, %.2f, %.2f)", cameraRot.x, cameraRot.y, cameraRot.z);
      }
   }

   // プレイヤー情報を表示
   if (ImGui::CollapsingHeader("Player Info")) {
      if (player) {
         Vector3 playerPos = player->GetWorldPosition();
         ImGui::Text("Position: (%.2f, %.2f, %.2f)", playerPos.x, playerPos.y, playerPos.z);
         ImGui::Text("Model Pointer: %p", (void*)player->GetModel());
         ImGui::Text("Is Spinning: %s", player->IsSpinning() ? "YES" : "NO");
      }
   }

   // 惑星情報を動的に表示
   for (size_t i = 0; i < planets.size(); ++i) {
      std::string header = "Planet " + std::to_string(i + 1) + " Info";
      if (ImGui::CollapsingHeader(header.c_str())) {
         if (planets[i]) {
            Vector3 planetPos = planets[i]->GetWorldPosition();
            ImGui::Text("Position: (%.2f, %.2f, %.2f)", planetPos.x, planetPos.y, planetPos.z);
            ImGui::Text("Gravity Radius: %.2f", planets[i]->GetGravitationalRadius());
            ImGui::Text("Planet Radius: %.2f", planets[i]->GetPlanetRadius());
            ImGui::Text("Model Pointer: %p", (void*)planets[i]->GetModel());
         }
      }
   }

   if (ImGui::CollapsingHeader("Star Info")) {
      if (star) {
         Vector3 starPos = star->GetWorldPosition();
         ImGui::Text("Position: (%.2f, %.2f, %.2f)", starPos.x, starPos.y, starPos.z);
         ImGui::Text("Star Radius: %.2f", star->GetStarRadius());
         ImGui::Text("Model Pointer: %p", (void*)star->GetModel());
      }
   }

   if (ImGui::CollapsingHeader("Rabbits Info")) {
      ImGui::Text("Active Rabbits: %zu", rabbits.size());
      for (size_t i = 0; i < rabbits.size(); ++i) {
         if (rabbits[i]) {
            Vector3 rabbitPos = rabbits[i]->GetWorldPosition();
            ImGui::Text("Rabbit %zu: (%.2f, %.2f, %.2f)", i + 1, rabbitPos.x, rabbitPos.y, rabbitPos.z);
            ImGui::Text("Model Pointer: %p", (void*)rabbits[i]->GetModel());
         }
      }
   }
   
   if (ImGui::CollapsingHeader("Boxes Info")) {
      ImGui::Text("Active Boxes: %zu", boxes.size());
      for (size_t i = 0; i < boxes.size(); ++i) {
         if (boxes[i]) {
            Vector3 boxPos = boxes[i]->GetWorldPosition();
            ImGui::Text("Box %zu: (%.2f, %.2f, %.2f)", i + 1, boxPos.x, boxPos.y, boxPos.z);
            ImGui::Text("Size: %.2f", boxes[i]->GetBoxSize());
            ImGui::Text("Model Pointer: %p", (void*)boxes[i]->GetModel());
         }
      }
   }

   ImGui::End();
#endif
}

void GameSceneUpdater::UpdateTimer(float dt, TimerDisplay* timerDisplay) {
   if (timerDisplay) {
      timerDisplay->Update(dt);
   }
}
