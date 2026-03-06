#include "GameScene.h"
#include <numbers>
#include <algorithm>
#include "UI/ImGuiManager.h"
#include "ObjectEdit.h"
#include "Framework/EngineContext.h"
#include "Asset/AssetManager.h"
#include "Utility/MathUtils.h"
#include "Utility/MathUtils/EasingFunctions.h"
#include "Utility/StateMachine.h"
#include "Effect/ParticleSystemEdit.h"
#include "LevelEditor.h"
#include "GameSceneInitializer.h"
#include "GameSceneUpdater.h"
#include "GameSceneRenderer.h"
#include "GameSceneLevelLoader.h"
#include "../UI/TimerDisplay.h"
#include "../GameObject/Box/Box.h"
#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui.h"
#endif

using namespace GameEngine;

GameScene::GameScene() {}

void GameScene::Initialize() {
   BaseScene::Initialize();

   // ライトマネージャーから取得して設定
   auto directionalLight = EngineContext::GetDirectionalLight("MainDirectionalLight");
   if (directionalLight) {
      directionalLight->GetDirectionalLightData()->intensity = 0.0f;
   }

   auto pointLight = EngineContext::GetPointLight("MainPointLight");
   if (pointLight) {
      pointLight->GetPointLightData()->intensity = 2.5f;
      pointLight->GetPointLightData()->position = Vector3(200.0f, 70.0f, -147.5f);
      pointLight->GetPointLightData()->radius = 500.0f;
	  pointLight->GetPointLightData()->decay = 1.0f;
   }

   // フェードの初期化（黒フェード、1.5秒、EaseInOut）
   CreateDefaultFade(1.5f, 0x000000ff);
   if (sceneFade_) {
	  sceneFade_->SetEasingType(SceneFade::EasingType::EaseInOut);
	  sceneFade_->SetEasingPower(2.0f);
   }

   // レベルエディターの作成
   levelEditor_ = std::make_unique<LevelEditor>();

   // 各種初期化を関数に分割
   GameSceneInitializer::LoadTextures();
   GameSceneInitializer::LoadModels();
   auto sunMat = EngineContext::GetMaterial("SkydomeMaterial");
   auto sunAsset = EngineContext::GetModel("planet.obj");
   sunModel_ = std::make_unique<Model>();
   sunModel_->Create(sunAsset, sunMat);
   sunModel_->SetScale(Vector3(5.0f, 5.0f, 5.0f));
   sunModel_->SetPosition(Vector3(200.0f, 70.0f, -147.5f));
   GameSceneInitializer::CreateMaterials();
   GameSceneInitializer::InitializeModels(playerModel_, starModel_, rabbitModelAsset_, skyDomeModel_, star_, isStarActive_);
   GameSceneInitializer::InitializeParticleSystems(particleSmoke_, particleStar_, particleDust_, particleStarCollection_, particleRabbitCapture_);
   GameSceneInitializer::InitializeCollisionSystem(collisionConfig_, collisionManager_);
   CreateLevelObjects();
   InitializePlayer();
   InitializeCamera();
   GameSceneInitializer::InitializeUI(cameraUISprite_, moveUISprite_, jumpUISprite_, spinUISprite_, howToPlaySprite_);

   // タイマー表示の初期化
   timerDisplay_ = std::make_unique<TimerDisplay>();
   timerDisplay_->Initialize(Vector2(540.0f, -48.0f), Vector2(40.0f, 60.0f)); // 画面上部中央付近
   timerDisplay_->SetValue(gameTimer_);

   GameSceneInitializer::InitializeCameraSequence(openingSequencer_, orbitalCamera_, cameraEditor_, mainCamera_.get(), openingSequenceFile_);

   // カメラシーケンスのコールバック設定
   if (openingSequencer_) {
	  openingSequencer_->SetOnCompleteCallback([this]() {
		 StartOrbitalCamera();
		 });
	  openingSequencer_->SetOnKeyframeChangeCallback([](size_t /*index*/) {});
   }

   // ステートマシンの初期化（全ての初期化が完了した後）
   InitializeStateMachine();

   EngineContext::SetPostProcessEffectEnabled("Vignette", true);
   EngineContext::SetPostProcessEffectEnabled("Gauss Blur", true);
   EngineContext::SetPostProcessEffectEnabled("Bloom", true);
   EngineContext::SetPostProcessEffectEnabled("Chromatic Aberration", true);

   EngineContext::LoadSound("resources/sounds/bgm/stage.mp3", "bgmStage");
   EngineContext::LoadSound("resources/sounds/se/jump.mp3", "seJump");
   EngineContext::LoadSound("resources/sounds/se/star.mp3", "seStar");
   EngineContext::LoadSound("resources/sounds/se/spin.mp3", "seSpin");
   auto bgm = EngineContext::GetSound("bgmTitle");
   if (bgm) {
	  bgm->Stop();
   }

   bgm = EngineContext::GetSound("bgmStage");
   if (bgm) {
	  bgm->Play(0.5f, true);
   }
}

// ===== カメラシーケンス関連 =====

void GameScene::StartOrbitalCamera() {
   if (!orbitalCamera_) return;

   // 軌道カメラ開始前にフェードインを開始（1.5秒、EaseInOut）
   if (sceneFade_) {
	  sceneFade_->SetFadeDuration(1.5f);
	  sceneFade_->SetFadeColor(0x000000ff);
	  sceneFade_->SetEasingType(SceneFade::EasingType::EaseInOut);
	  sceneFade_->SetEasingPower(2.0f);
	  sceneFade_->ResetFadeOutCompleted();
	  sceneFade_->StartFadeIn();
   }

   // 軌道パラメータを設定（原点を中心に360度回転）
   OrbitalCameraController::OrbitParams params;
   params.targetPosition = Vector3(0.0f, 3.0f, 0.0f);  // 原点を注視
   params.radius = 75.0f;                              // 半径75の軌道
   params.startAngleY = -1.57f;                        // -90度（左側）から開始
   params.endAngleY = 3.71f;                           // 270度（360度回転）
   params.startAngleX = -0.4f;                          // 少し上から
   params.endAngleX = 0.4f;                            // 少し下へ
   params.duration = 2.5f;                             // 2秒で1周
   params.easingType = CameraKeyframe::EasingType::EaseInOut;
   params.easingPower = 2.0f;
   params.fov = 0.45f;                                  // 広角
   params.lookAtTarget = true;                         // 常に中心を見る

   orbitalCamera_->SetOrbitParams(params);

   // 完了時のコールバック
   orbitalCamera_->SetOnCompleteCallback([this]() {
	  // 軌道カメラ完了後、フェードアウト（1.5秒、EaseInOut）してから通常のゲームプレイへ
	  if (sceneFade_) {
		 sceneFade_->SetFadeDuration(1.5f);
		 sceneFade_->SetFadeColor(0x000000ff);
		 sceneFade_->SetEasingType(SceneFade::EasingType::EaseInOut);
		 sceneFade_->SetEasingPower(2.0f);
		 sceneFade_->ResetFadeOutCompleted();
		 sceneFade_->StartFadeOut();
	  }

	  // 軌道カメラ停止
	  isOrbitalCameraPlaying_ = false;

	  // フェードアウト完了を待つ
	  isWaitingForOrbitalFadeOut_ = true;
	  });

   // 軌道カメラを即座に開始
   orbitalCamera_->Start(false);
   isOrbitalCameraPlaying_ = true;
}

// ===== ステートマシン関連 =====

void GameScene::InitializeStateMachine() {
   stateMachine_ = std::make_unique<StateMachine>();

   // 各ステートを登録
   stateMachine_->AddState("Opening",
	  [this]() { OnEnterOpening(); },
	  [this]() { UpdateOpening(); });

   stateMachine_->AddState("Playing",
	  [this]() { OnEnterPlaying(); },
	  [this]() { UpdatePlaying(); });

   stateMachine_->AddState("StarAvailable",
	  [this]() { OnEnterStarAvailable(); },
	  [this]() { UpdateStarAvailable(); });

   stateMachine_->AddState("StarActivationCutscene",
	  [this]() { OnEnterStarActivationCutscene(); },
	  [this]() { UpdateStarActivationCutscene(); });

   stateMachine_->AddState("StarCollectionCutscene",
	  [this]() { OnEnterStarCollectionCutscene(); },
	  [this]() { UpdateStarCollectionCutscene(); });

   // 遷移ルールを設定
   stateMachine_->AddTransitionRule("Opening", { "Playing" });
   stateMachine_->AddTransitionRule("Playing", { "StarActivationCutscene" });
   stateMachine_->AddTransitionRule("StarActivationCutscene", { "StarAvailable" });
   stateMachine_->AddTransitionRule("StarAvailable", { "StarCollectionCutscene" });
   stateMachine_->AddTransitionRule("StarCollectionCutscene", {});  // StarCollectionから遷移しない（フェードアウト後にClearSceneへ）

   // 初期状態をOpeningに設定
   stateMachine_->RequestState("Opening", 100);
}

void GameScene::OnEnterOpening() {
   stateTimer_ = 0.0f;

   // オープニングシーケンスを開始
   if (openingSequencer_) {
	  openingSequencer_->Play(false);  // ループなし
	  isOpeningSequencePlaying_ = true;

	  // シーンフェードにイージングを設定してフェードイン
	  if (sceneFade_) {
		 sceneFade_->SetEasingType(GameEngine::SceneFade::EasingType::EaseInOut);
	  }
	  sceneFade_->StartFadeIn();
   }
}

void GameScene::UpdateOpening() {
   stateTimer_ += EngineContext::GetDeltaTime();

   // キーフレームシーケンスの更新
   if (openingSequencer_ && isOpeningSequencePlaying_ && !isOrbitalCameraPlaying_) {
	  openingSequencer_->Update();
   }

   // 軌道カメラの更新
   if (orbitalCamera_ && isOrbitalCameraPlaying_ && !isWaitingForOrbitalFadeOut_) {
	  orbitalCamera_->Update();
   }

   // 軌道カメラ終了後のフェードアウト待機
   if (isWaitingForOrbitalFadeOut_) {
	  // フェードアウトが完了するまで待機（フェードアウト完了フラグで判定）
	  if (sceneFade_ && sceneFade_->IsFadeOutCompleted()) {
		 isOpeningSequencePlaying_ = false;
		 isOrbitalCameraPlaying_ = false;
		 isWaitingForOrbitalFadeOut_ = false;

		 // Playingステートへ遷移
		 if (stateMachine_) {
			stateMachine_->RequestState("Playing", 100);
		 }
	  }
   }

   // シーケンサーがない場合は3秒後に遷移
   if (!openingSequencer_) {
	  if (stateTimer_ >= 3.0f) {
		 stateMachine_->RequestState("Playing", 100);
	  }
   }
}

void GameScene::OnEnterPlaying() {
   stateTimer_ = 0.0f;

   // TPSカメラコントローラーに制御を戻す
   isOpeningSequencePlaying_ = false;

   // タイマーを有効化
   isTimerActive_ = true;
   gameTimer_ = 300.0f;
   timerDisplay_->SetValue(gameTimer_);

   // HowToPlayアニメーションを開始
   isHowToPlayAnimeActive_ = true;
   howToPlayAnimeTime_ = 0.0f;

   // フェードインを開始（オープニングから遷移したとき、1.5秒、EaseInOut）
   if (sceneFade_) {
	  sceneFade_->SetFadeDuration(1.5f);
	  sceneFade_->SetEasingType(SceneFade::EasingType::EaseInOut);
	  sceneFade_->SetEasingPower(2.0f);
	  sceneFade_->StartFadeIn();
   }
}

void GameScene::UpdatePlaying() {
   stateTimer_ += EngineContext::GetDeltaTime();

   // HowToPlayアニメーションの更新
   if (isHowToPlayAnimeActive_) {
      howToPlayAnimeTime_ += EngineContext::GetDeltaTime();
      
      if (howToPlayAnimeTime_ >= kHowToPlayAnimeDuration_) {
         isHowToPlayAnimeActive_ = false;
      } else {
         UpdateHowToPlayAnimation();
      }
   }

   // タイマーのカウントダウン
   if (isTimerActive_) {
	  gameTimer_ -= EngineContext::GetDeltaTime();

	  // 0以下になったらゲームオーバー
	  if (gameTimer_ <= 0.0f) {
		 gameTimer_ = 0.0f;
		 isTimerActive_ = false;

		 // ゲームオーバーシーンに遷移
		 if (sceneFade_) {
			sceneFade_->SetFadeColor(0x000000ff);  // 白色フェード
			sceneFade_->SetFadeDuration(1.5f);
			sceneFade_->SetEasingType(SceneFade::EasingType::EaseInOut);
			sceneFade_->SetEasingPower(2.0f);
			sceneFade_->ResetFadeOutCompleted();
			sceneFade_->StartFadeOut();

			// フェードアウト完了後にGameOverシーンに遷移
			sPendingSceneName_ = "GameOver";
			sIsWaitingForFadeOut_ = true;

#ifdef USE_IMGUI
			OutputDebugStringA("GameScene: Time's up! Transitioning to GameOver scene\n");
#endif
		 } else {
			// フェードがない場合は直接遷移
		 sNextSceneName_ = "GameOver";
		 }

		 return;  // タイムアップ後は他の処理をスキップ
	  }

	  // タイマー表示を更新
	  if (timerDisplay_) {
		 timerDisplay_->SetValue(gameTimer_);
	  }
   }

   // スター獲得可能かチェック
   CheckStarActivation();
}

void GameScene::OnEnterStarActivationCutscene() {
   stateTimer_ = 0.0f;

   // スター獲得可能音を再生
   auto seStar = EngineContext::GetSound("seStar");
   if (seStar) {
	  seStar->Play(0.5f, false);
   }

   // スターアクティベーション演出を開始
   if (orbitalCamera_ && star_) {
	  // スターの位置を取得
	  Vector3 starPos = star_->GetWorldPosition();

	  // スターの色を黄色に変更し、パーティクルを有効化
	  UpdateStarColor();

	  // 軌道カメラのパラメータを設定（回転なし、スターを注視、水平視点）
	  OrbitalCameraController::OrbitParams params;
	  params.targetPosition = starPos;           // スターの位置を注視
	  params.radius = 7.0f;                      // スターから7.0fの距離
	  params.startAngleY = 0.0f;                 // 正面から
	  params.endAngleY = 0.0f;                   // 回転なし（同じ角度）
	  params.startAngleX = 0.0f;                 // 水平（真横から）
	  params.endAngleX = 0.0f;                   // 角度変化なし（水平を維持）
	  params.duration = 1.5f;                    // 1.5秒間（スターの回転演出時間に合わせる）
	  params.easingType = CameraKeyframe::EasingType::Linear;
	  params.easingPower = 2.0f;
	  params.fov = 0.45f;
	  params.lookAtTarget = true;                // 常にスターを見る

	  orbitalCamera_->SetOrbitParams(params);

	  // 完了時のコールバック
	  orbitalCamera_->SetOnCompleteCallback([this]() {
		 // 軌道カメラ停止
		 isStarActivationSequencePlaying_ = false;

		 // StarAvailableステートに遷移
		 if (stateMachine_) {
			stateMachine_->RequestState("StarAvailable", 100);
		 }
		 });

	  // スターの演出を開始
	  star_->StartActivationEffect();

	  // 軌道カメラを開始
	  orbitalCamera_->Start(false);
	  isStarActivationSequencePlaying_ = true;
   }
}

void GameScene::UpdateStarActivationCutscene() {
   stateTimer_ += EngineContext::GetDeltaTime();
   float dt = EngineContext::GetDeltaTime();

   // タイマーのカウントダウン（演出中も継続）
   if (isTimerActive_) {
	  gameTimer_ -= dt;

	  if (gameTimer_ <= 0.0f) {
		 gameTimer_ = 0.0f;
		 isTimerActive_ = false;

		 // ゲームオーバーシーンに遷移
		 if (sceneFade_) {
			sceneFade_->SetFadeColor(0xffffffff);
			sceneFade_->SetFadeDuration(1.5f);
			sceneFade_->SetEasingType(SceneFade::EasingType::EaseInOut);
			sceneFade_->SetEasingPower(2.0f);
			sceneFade_->ResetFadeOutCompleted();
			sceneFade_->StartFadeOut();

			sPendingSceneName_ = "GameOver";
			sIsWaitingForFadeOut_ = true;

#ifdef USE_IMGUI
			OutputDebugStringA("GameScene: Time's up during cutscene! Transitioning to GameOver scene\n");
#endif
		 }

		 return;
	  }

	  if (timerDisplay_) {
		 timerDisplay_->SetValue(gameTimer_);
	  }
   }

   // 軌道カメラの更新
   if (orbitalCamera_ && isStarActivationSequencePlaying_) {
	  orbitalCamera_->Update();
   }
}

void GameScene::OnEnterStarAvailable() {
   stateTimer_ = 0.0f;

   // スターを有効化
   ActivateStar();
}

void GameScene::UpdateStarAvailable() {
   stateTimer_ += EngineContext::GetDeltaTime();
   float dt = EngineContext::GetDeltaTime();

   // タイマーのカウントダウン
   if (isTimerActive_) {
	  gameTimer_ -= dt;

	  if (gameTimer_ <= 0.0f) {
		 gameTimer_ = 0.0f;
		 isTimerActive_ = false;

		 // ゲームオーバーシーンに遷移
		 if (sceneFade_) {
			sceneFade_->SetFadeColor(0xffffffff);
			sceneFade_->SetFadeDuration(1.5f);
			sceneFade_->SetEasingType(SceneFade::EasingType::EaseInOut);
			sceneFade_->SetEasingPower(2.0f);
			sceneFade_->ResetFadeOutCompleted();
			sceneFade_->StartFadeOut();

			sPendingSceneName_ = "GameOver";
		 sIsWaitingForFadeOut_ = true;

#ifdef USE_IMGUI
			OutputDebugStringA("GameScene: Time's up! Transitioning to GameOver scene\n");
#endif
		 }

		 return;
	  }

	  if (timerDisplay_) {
		 timerDisplay_->SetValue(gameTimer_);
	  }
   }

   // スターが獲得されたかチェック（演出が開始されたらステート遷移）
   if (star_ && star_->IsPlayingCollectionEffect() && !isStarCollectionSequencePlaying_) {
	  // StarCollectionCutsceneステートに遷移
	  if (stateMachine_) {
		 stateMachine_->RequestState("StarCollectionCutscene", 100);
	  }
   }
}

void GameScene::OnEnterStarCollectionCutscene() {
   stateTimer_ = 0.0f;
   isStarCollectionSequencePlaying_ = true;

   // クリア演出音を再生
   auto seStar = EngineContext::GetSound("seStar");
   if (seStar) {
	  seStar->Play(0.5f, false);
   }

   // プレイヤーの移動パーティクルを停止
   if (particleSmoke_ && particleSmoke_->GetEmissionModule()) {
	  particleSmoke_->GetEmissionModule()->SetEnabled(false);
   }

   // スター獲得演出用のカメラシーケンスを開始
   if (orbitalCamera_ && star_) {
	  // スターの演出開始位置を取得
	  Vector3 starStartPos = star_->GetCollectionStartPosition();

	  // 軌道カメラのパラメータを設定
	  // 注視点はスターの初期位置で固定、カメラはイーズインで引く
	  OrbitalCameraController::OrbitParams params;
	  params.targetPosition = starStartPos;      // スターの開始位置を注視し続ける
	  params.radius = 7.0f;                      // 開始距離（近い）
	  params.startAngleY = 0.0f;                 // 正面から
	  params.endAngleY = 0.0f;                   // 回転なし
	  params.startAngleX = 0.0f;                 // 水平視点
	  params.endAngleX = 0.0f;                   // 角度変化なし
	  params.duration = 1.0f;                    // 1秒間に変更
	  params.easingType = CameraKeyframe::EasingType::Linear;  // Linearで開始（radiusは手動でイーズイン）
	  params.easingPower = 1.0f;
	  params.fov = 0.45f;
	  params.lookAtTarget = true;                // 常にスターの初期位置を見続ける

	  orbitalCamera_->SetOrbitParams(params);

	  // 完了時のコールバック（使用しない - フェードアウト後に遷移）
	  orbitalCamera_->SetOnCompleteCallback([]() {
		 // 何もしない
		 });

	  // 軌道カメラを開始
	  orbitalCamera_->Start(false);
   }
}

void GameScene::UpdateStarCollectionCutscene() {
   stateTimer_ += EngineContext::GetDeltaTime();
   float dt = EngineContext::GetDeltaTime();

   // タイマーのカウントダウン（演出中も継続）
   if (isTimerActive_) {
	  gameTimer_ -= dt;

	  if (gameTimer_ <= 0.0f) {
		 gameTimer_ = 0.0f;
		 isTimerActive_ = false;

		 // ゲームオーバーシーンに遷移（演出中でも時間切れ優先）
		 if (sceneFade_ && sceneFade_->GetFadeState() != GameEngine::SceneFade::FadeState::FadeOut && !sceneFade_->IsFadeOutCompleted()) {
			sceneFade_->SetFadeColor(0xffffffff);
			sceneFade_->SetFadeDuration(1.5f);
			sceneFade_->SetEasingType(GameEngine::SceneFade::EasingType::EaseInOut);
			sceneFade_->SetEasingPower(2.0f);
			sceneFade_->ResetFadeOutCompleted();
			sceneFade_->StartFadeOut();

			sPendingSceneName_ = "GameOver";
			sIsWaitingForFadeOut_ = true;

#ifdef USE_IMGUI
			OutputDebugStringA("GameScene: Time's up during collection cutscene! Transitioning to GameOver scene\n");
#endif
		 }

		 return;
	  }

	  if (timerDisplay_) {
		 timerDisplay_->SetValue(gameTimer_);
	  }
   }

   // 軌道カメラの更新
   if (orbitalCamera_ && isStarCollectionSequencePlaying_) {
	  // カメラを徐々に引く演出（radiusを7.0f -> 20.0fにイーズインで変化）
	  float progress = stateTimer_ / 1.0f;  // 1.0秒間に変更

	  if (progress < 1.0f) {
		 // Cubic ease-in
		 float easedProgress = progress * progress * progress;
		 float currentRadius = 7.0f + (20.0f - 7.0f) * easedProgress;

		 // OrbitalCameraのradiusを動的に設定
		 orbitalCamera_->SetCurrentRadius(currentRadius);
	  } else {
		 // 演出完了 - フェードアウト開始
		 if (sceneFade_ && sceneFade_->GetFadeState() != GameEngine::SceneFade::FadeState::FadeOut && !sceneFade_->IsFadeOutCompleted()) {
			// 白色フェードアウトを開始してClearSceneに遷移
			sceneFade_->SetFadeColor(0xffffffff);  // 白色フェード
			sceneFade_->SetFadeDuration(1.5f);
			sceneFade_->SetEasingType(GameEngine::SceneFade::EasingType::EaseInOut);
			sceneFade_->SetEasingPower(2.0f);
			sceneFade_->ResetFadeOutCompleted();
			sceneFade_->StartFadeOut();

			// フェードアウト完了後にClearSceneに遷移
		 sPendingSceneName_ = "Clear";
		 sIsWaitingForFadeOut_ = true;

#ifdef USE_IMGUI
			OutputDebugStringA("Started fade out to Clear scene\n");
#endif
		 }
	  }

	  // OrbitalCameraを更新
	  orbitalCamera_->Update();
   }
}

// ===== 更新関連 =====

void GameScene::Update() {
   // フェードの更新を先に行う（BaseScene::Update()を呼ばない場合でも必要）
   if (sceneFade_) {
	  sceneFade_->Update();

	  // フェードアウトが完了したらシーン切り替えフラグを立てる
	  if (sIsWaitingForFadeOut_ && sceneFade_->IsFadeOutCompleted()) {
		 sNextSceneName_ = sPendingSceneName_;
		 sIsWaitingForFadeOut_ = false;
		 sPendingSceneName_ = "";
	  }
   }

   // ステートマシンの更新
   if (stateMachine_) {
	  stateMachine_->Update();
   }

   UpdateDebugFeatures();
   UpdateCameraSequence();
   UpdateCamera();
   UpdateTimer();
   UpdateGameObjects();
   UpdateCollisions();
   ProcessRabbitRemoval();
   UpdateParticles();
   UpdateDebugUI();
}

void GameScene::UpdateDebugFeatures() {
#ifdef USE_IMGUI
   GameSceneUpdater::UpdateDebugFeatures(
	  levelEditor_.get(),
	  autoReload_,
	  currentLevelFile_,
	  isDebugCameraActive_,
	  mainCamera_.get(),
	  mainCameraPrevTransform_,
	  [this]() { LoadLevelFromEditor(); },
	  [this]() { ExportLevelToEditor(); }
   );
#endif
}

void GameScene::UpdateCameraSequence() {
   GameSceneUpdater::UpdateCameraSequence(
	  cameraEditor_.get(),
	  openingSequencer_.get(),
	  orbitalCamera_.get(),
	  mainCamera_.get(),
	  openingSequenceFile_,
	  isOpeningSequencePlaying_,
	  isOrbitalCameraPlaying_,
	  isWaitingForOrbitalFadeOut_,
	  orbitalCameraEndTimer_,
	  stateMachine_.get(),
	  sceneFade_.get(),
	  [this]() { StartOrbitalCamera(); }
   );
}

void GameScene::UpdateCamera() {
#ifdef USE_IMGUI
   GameSceneUpdater::UpdateCamera(
	  isDebugCameraActive_,
	  isOpeningSequencePlaying_ || isStarActivationSequencePlaying_ || isStarCollectionSequencePlaying_,
	  isOrbitalCameraPlaying_ || isStarActivationSequencePlaying_ || isStarCollectionSequencePlaying_,
	  debugCamera_.get(),
	  tpsCameraController_.get()
   );
#else
   GameSceneUpdater::UpdateCamera(
	  false,
	  isOpeningSequencePlaying_ || isStarActivationSequencePlaying_ || isStarCollectionSequencePlaying_,
	  isOrbitalCameraPlaying_ || isStarActivationSequencePlaying_ || isStarCollectionSequencePlaying_,
	  nullptr,
	  tpsCameraController_.get()
   );
#endif
}

void GameScene::UpdateGameObjects() {
   float dt = EngineContext::GetDeltaTime();
   
   // プレイヤーの更新を先に実行（オープニング中は停止）
   if (player_ && !isOpeningSequencePlaying_ && !isStarActivationSequencePlaying_ && !isOrbitalCameraPlaying_) {
      player_->Update(dt);
   }

   // 全惑星の更新
   for (auto& planet : planets_) {
      if (planet) {
         planet->Update(dt);
      }
   }

   if (star_) {
      star_->Update(dt);
   }

   // 全ラビットの更新
   for (auto& rabbit : rabbits_) {
      if (rabbit) {
         rabbit->Update(dt);
      }
   }
   
   // 全ボックスの更新
   for (auto& box : boxes_) {
      if (box) {
         box->Update(dt);
      }
   }
}

void GameScene::UpdateCollisions() {
   GameSceneUpdater::UpdateCollisions(collisionManager_.get());
}

void GameScene::ProcessRabbitRemoval() {
   GameSceneUpdater::ProcessRabbitRemoval(
	  rabbitsToRemove_,
	  rabbits_,
	  rabbitModels_,
	  collisionManager_.get(),
	  planets_,
	  player_.get(),
	  star_.get(),
	  isStarActive_,
	  boxes_
   );
}

void GameScene::UpdateParticles() {
   float dt = EngineContext::GetDeltaTime();

   GameSceneUpdater::UpdateParticles(
	  dt,
	  particleSmoke_.get(),
	  particleStar_.get(),
	  particleDust_.get(),
	  particleStarCollection_.get(),
	  particleRabbitCapture_.get()
   );

   // スターパーティクルをスターの位置に追従させる（毎フレーム更新）
   if (star_) {
	  Vector3 starPosition = star_->GetWorldPosition();

	  // アクティベーション用パーティクル（particleStar_）
	  if (particleStar_ && particleStar_->GetShapeModule() && isStarActive_) {
		 particleStar_->GetShapeModule()->SetPosition(starPosition);
	  }

	  // コレクション用パーティクル（particleStarCollection_）
	  if (particleStarCollection_ && particleStarCollection_->GetShapeModule() && star_->IsPlayingCollectionEffect()) {
		 particleStarCollection_->GetShapeModule()->SetPosition(starPosition);
	  }
   }
}
void GameScene::UpdateDebugUI() {
   GameSceneUpdater::UpdateDebugUI(
	  stateMachine_.get(),
	  stateTimer_,
	  rabbitsCollected_,
	  rabbitsRequired_,
	  isStarActive_,
	  mainCamera_.get(),
	  player_.get(),
	  planets_,
	  star_.get(),
	  rabbits_,
	  boxes_
   );
}

void GameScene::Draw() {
   // 軌道カメラのフェードアウト待機中でも、ゲームオブジェクトを描画し続ける
   DrawUI();
   DrawGameObjects();
   DrawParticles();
   DrawDebug();

   // カメラシーケンスのフェードを描画
   if (openingSequencer_ && isOpeningSequencePlaying_ && !isOrbitalCameraPlaying_) {
	  openingSequencer_->DrawFade();
   }

   // シーンフェードを描画（最後に描画して他のものの上に重ねる）
   BaseScene::Draw();
}

void GameScene::DrawUI() {
   // 演出中はUIを非表示にする
   bool showUI = !isOpeningSequencePlaying_ && !isOrbitalCameraPlaying_ && !isStarActivationSequencePlaying_ && !isStarCollectionSequencePlaying_;

   GameSceneRenderer::DrawUI(
	  moveUISprite_.get(),
	  jumpUISprite_.get(),
	  cameraUISprite_.get(),
	  spinUISprite_.get(),
	  showUI
   );

   // タイマー表示（演出中は非表示）
   if (timerDisplay_ && showUI) {
	  timerDisplay_->Draw();
   }
   
   // HowToPlayスプライトの描画（アニメーション中のみ）
   if (isHowToPlayAnimeActive_ && howToPlaySprite_) {
      auto howToPlayTex = EngineContext::GetTexture("howToPlay");
      if (howToPlayTex) {
         EngineContext::DrawUI(
            howToPlaySprite_.get(),
            howToPlayTex,
            GameEngine::Sprite::AnchorPoint::MiddleCenter,
            BlendMode::kBlendModeNormal
         );
      }
   }
}

void GameScene::DrawGameObjects() {
   // スター獲得演出中はプレイヤーを非表示
   bool showPlayer = !isStarCollectionSequencePlaying_;

   GameSceneRenderer::DrawGameObjects(
	  playerModel_.get(),
	  starModel_.get(),
	  rabbits_,
	  planets_,
	  boxes_,
	  showPlayer
   );
   
   // 太陽の描画
   auto sunTex = EngineContext::GetTexture("white1x1");
   if (sunModel_ && sunTex) {
	  EngineContext::SetActiveCamera(1);
	  EngineContext::SetBlendMode(BlendMode::kBlendModeNone);
	  EngineContext::BeginLensFlareOcclusionQuery();
	  EngineContext::Draw(sunModel_.get(), sunTex);
	  EngineContext::EndLensFlareOcclusionQuery();
   }

   auto skyTex = EngineContext::GetTexture("skydome");
   if (skyDomeModel_ && skyTex) {
	  EngineContext::SetActiveCamera(1);
	  EngineContext::SetBlendMode(BlendMode::kBlendModeNone);
	  EngineContext::Draw(skyDomeModel_.get(), skyTex);
   }
}

void GameScene::DrawParticles() {
   // スター獲得演出中はプレイヤーの移動パーティクル（煙）を非表示
   bool showPlayerParticles = !isStarCollectionSequencePlaying_;

   GameSceneRenderer::DrawParticles(
	  particleSmoke_.get(),
	  particleStar_.get(),
	  particleDust_.get(),
	  particleStarCollection_.get(),
	  particleRabbitCapture_.get(),
	  showPlayerParticles
   );
}

void GameScene::DrawDebug() {
   GameSceneRenderer::DrawDebug(levelEditor_.get());
}

// ===== ゲームロジック関連 =====

void GameScene::CheckStarActivation() {
   // 必要数集めたらスターアクティベーションカットシーンへ遷移
   if (rabbitsCollected_ >= rabbitsRequired_ && !isStarActive_ && stateMachine_) {
	  stateMachine_->RequestState("StarActivationCutscene", 100);
   }
}

void GameScene::ActivateStar() {
   isStarActive_ = true;

   // スターのコリダーを登録
   if (star_ && star_->GetCollider()) {
	  collisionManager_->RegisterCollider(star_->GetCollider());
   }

   // スターにパーティクルシステムを設定
   if (star_) {
	  if (particleStarCollection_) {
		 star_->SetParticleSystem(particleStarCollection_.get());
	  }

	  // スターアクティベーション用パーティクルを設定
	  if (particleStar_) {
		 star_->SetActivationParticleSystem(particleStar_.get());
	  }

	  // スター獲得時のコールバックを設定
	  star_->SetCollectionCallback([this]() {
#ifdef USE_IMGUI
		 OutputDebugStringA("=== Star Collection Callback ===\n");
#endif
		 // スターの獲得演出を開始（コールバックは即座に実行される）
		 // ステート遷移は UpdateStarAvailable() で star_->IsPlayingCollectionEffect() をチェックして行う

		 // 注: フェードアウトとシーン遷移はStarCollectionCutsceneステートの終了後に行う
		 });
   }
}

void GameScene::UpdateHowToPlayAnimation() {
   if (!howToPlaySprite_) return;
   
   float progress = howToPlayAnimeTime_ / kHowToPlayAnimeDuration_;
   progress = std::clamp(progress, 0.0f, 1.0f);
   
   // 画面の右端から左端へ移動（1280pxの範囲）
   // 中央で減速するため、2つのフェーズに分ける
   float currentX;
   
   if (progress < 0.5f) {
      // 前半: 右端(1280) -> 中央(0) をEaseOutで減速
      float halfProgress = progress * 2.0f; // 0.0 -> 1.0にマップ
      currentX = Easing::EaseOut(1280.0f, 0.0f, halfProgress, 3.0f);
   } else {
      // 後半: 中央(0) -> 左端(-1280) をEaseInで加速
      float halfProgress = (progress - 0.5f) * 2.0f; // 0.0 -> 1.0にマップ
      currentX = Easing::EaseIn(0.0f, -1280.0f, halfProgress, 3.0f);
   }
   
   howToPlaySprite_->SetPosition(Vector2(currentX, 0.0f));
}

void GameScene::Finalize() {
   BaseScene::Finalize();
}

void GameScene::CreateLevelObjects() {
   // デフォルトレベルをJSONから読み込む
   if (levelEditor_->LoadFromJson("resources/levels/default_level.json")) {
	  LoadLevelFromEditor();
   } else {
	  // JSONの読み込みに失敗した場合は従来のハードコードされた配置を使用
	  GameSceneLevelLoader::CreateDefaultLevel(
		 planets_,
		 planetModels_,
		 [this](const Vector3& pos, float pRadius, float gRadius) {
			CreatePlanet(pos, pRadius, gRadius);
		 }
	  );
   }
}

void GameScene::InitializePlayer() {
   // プレイヤーの作成
   player_ = std::make_unique<Player>();
   player_->Initialize(playerModel_.get(), 0.65f);

   // 全惑星のリストを設定（最近接惑星検索用）
   std::vector<Planet*> planetPointers;
   for (auto& planet : planets_) {
	  planetPointers.push_back(planet.get());
   }
   player_->SetAllPlanets(planetPointers);

   // 移動コールバック：移動中のみパーティクルを放出
   player_->SetMoveCallback([this](const Vector3& pos) {
	  if (player_->GetCurrentPlanet()) {
		 Vector3 planetCenter = player_->GetCurrentPlanet()->GetWorldPosition();
		 Vector3 headDir = (pos - planetCenter).Normalize();
		 Vector3 footPosition = pos - headDir * 0.6f;

		 particleSmoke_->GetShapeModule()->SetPosition(footPosition);

		 if (!particleSmoke_->IsPlaying()) {
			particleSmoke_->Play();
		 }

		 particleSmoke_->GetEmissionModule()->SetEnabled(true);
	  }
	  });

   player_->SetStopCallback([this]() {
	  if (particleSmoke_->GetEmissionModule()) {
		 particleSmoke_->GetEmissionModule()->SetEnabled(false);
	  }
	  });

   // ラビット捕獲コールバック：ラビットに当たったら削除
   player_->SetRabbitCaptureCallback([this](Rabbit* rabbit) {
	  RemoveRabbit(rabbit);
	  });

   // プレイヤーに惑星を直接設定（惑星が存在する場合）
   if (!planets_.empty()) {
	  player_->SetCurrentPlanet(planets_[0].get());
   }

   // 全てのラビットにプレイヤーポイントを設定
   for (auto& rabbit : rabbits_) {
	  if (rabbit) {
		 rabbit->SetPlayer(player_.get());
	  }
   }

   // コライダーを登録
   for (auto& planet : planets_) {
	  if (planet->GetCollider()) {
		 collisionManager_->RegisterCollider(planet->GetCollider());
	  }
   }

   if (player_->GetCollider()) {
	  collisionManager_->RegisterCollider(player_->GetCollider());
   }
   
   // プレイヤーのスピンコライダーも登録（初期状態では無効）
   if (player_->GetSpinCollider()) {
	  collisionManager_->RegisterCollider(player_->GetSpinCollider());
   }

   for (auto& rabbit : rabbits_) {
	  if (rabbit->GetCollider()) {
		 collisionManager_->RegisterCollider(rabbit->GetCollider());
	  }
   }
   
   for (auto& box : boxes_) {
	  if (box->GetCollider()) {
		 collisionManager_->RegisterCollider(box->GetCollider());
	  }
   }

   // 必要なラビット数を設定
   rabbitsRequired_ = static_cast<int>(rabbits_.size());
}

void GameScene::InitializeCamera() {
   // TPSカメラコントローラーの作成と初期化
   tpsCameraController_ = std::make_unique<TPSCameraController>();
   tpsCameraController_->Initialize(mainCamera_.get(), player_.get());

   // カメラに惑星リストを設定（衝突判定用）
   std::vector<Planet*> planetPointers;
   for (auto& planet : planets_) {
	  planetPointers.push_back(planet.get());
   }
   tpsCameraController_->SetPlanets(planetPointers);

   // プレイヤーにカメラコントローラーを設定
   player_->SetCameraController(tpsCameraController_.get());
}

void GameScene::CreatePlanet(const Vector3& position, float planetRadius, float gravitationalRadius) {
   GameSceneLevelLoader::CreatePlanet(
	  position,
	  planetRadius,
	  gravitationalRadius,
	  planets_,
	  planetModels_
   );
}

void GameScene::CreateRabbit(size_t planetIndex, const GameEngine::Vector3& offset, float radius) {
   GameSceneLevelLoader::CreateRabbit(
	  planetIndex,
	  offset,
	  radius,
	  planets_,
	  rabbits_,
	  rabbitModels_,
	  player_.get(),
	  particleRabbitCapture_.get()
   );
   
   // 最後に作成されたラビットにコールバックを設定
   if (!rabbits_.empty() && rabbits_.back()) {
	  auto* rabbit = rabbits_.back().get();
	  
	  // 捕獲時のコールバックを設定（パーティクルを再生）
	  rabbit->SetCaptureCallback([this](const Vector3& position) {
		 if (particleRabbitCapture_) {
			particleRabbitCapture_->GetShapeModule()->SetPosition(position);

			if (!particleRabbitCapture_->IsPlaying()) {
			   particleRabbitCapture_->Play();
			}

			if (particleRabbitCapture_->GetEmissionModule()) {
			   particleRabbitCapture_->GetEmissionModule()->SetEnabled(true);
			}
		 }
	  });
   }
}

void GameScene::CreateBox(size_t planetIndex, const GameEngine::Vector3& offset, float size, float mass) {
   // massは無視される
   (void)mass;
   
   GameSceneLevelLoader::CreateBox(
	  planetIndex,
	  offset,
	  size,
	  mass,
	  planets_,
	  boxes_,
	  boxModels_,
	  nullptr  // playerは不要
   );
}

void GameScene::PlaceStar(size_t planetIndex, const GameEngine::Vector3& offset, float radius) {
   GameSceneLevelLoader::PlaceStar(
	  planetIndex,
	  offset,
	  radius,
	  planets_,
	  star_.get(),
	  starModel_.get(),
	  particleStar_.get()
   );
}

void GameScene::RemoveRabbit(Rabbit* rabbit) {
   // ラビットのインデックスを見つける
   auto it = std::find_if(rabbits_.begin(), rabbits_.end(),
	  [rabbit](const std::unique_ptr<Rabbit>& r) {
		 return r.get() == rabbit;
	  });

   if (it != rabbits_.end()) {
	  // 重複チェック
	  auto existsInRemoveList = std::find(rabbitsToRemove_.begin(), rabbitsToRemove_.end(), rabbit);
	  if (existsInRemoveList != rabbitsToRemove_.end()) {
		 return;
	  }

	  // ラビット捕獲音を再生
	  auto seStar = EngineContext::GetSound("seStar");
	  if (seStar) {
		 seStar->Play(0.5f, false);
	  }

	  // 捕獲時のコールバックを設定（パーティクルを再生）
	  rabbit->SetCaptureCallback([this](const Vector3& position) {
		 if (particleRabbitCapture_) {
			particleRabbitCapture_->GetShapeModule()->SetPosition(position);

			if (!particleRabbitCapture_->IsPlaying()) {
			   particleRabbitCapture_->Play();
			}

			if (particleRabbitCapture_->GetEmissionModule()) {
			   particleRabbitCapture_->GetEmissionModule()->SetEnabled(true);
			}
		 }
		 });

	  // 捕獲演出を開始
	  rabbit->StartCaptureEffect();

	  // ラビットを削除予定リストに追加
	  rabbitsToRemove_.push_back(rabbit);

	  // 捕獲数をカウント
	  rabbitsCollected_++;
   }
}

void GameScene::UpdateStarColor() {
   // スターのマテリアルを黄色に変更
   auto starMat = EngineContext::GetMaterial("StarMaterial");
   if (starMat) {
	  starMat->SetColor(0xffff00ff);  // 黄色
   }

   // スターパーティクルを有効化
   if (particleStar_) {
	  // スターの位置にパーティクルエミッターを設定
	  if (star_ && particleStar_->GetShapeModule()) {
		 Vector3 starPosition = star_->GetWorldPosition();
		 particleStar_->GetShapeModule()->SetPosition(starPosition);
	  }

	  if (!particleStar_->IsPlaying()) {
		 particleStar_->Play();
	  }

	  if (particleStar_->GetEmissionModule()) {
		 particleStar_->GetEmissionModule()->SetEnabled(true);
	  }
   }
}

void GameScene::LoadLevelFromEditor() {
   GameSceneLevelLoader::LoadLevelFromEditor(
	  levelEditor_.get(),
	  planets_,
	  planetModels_,
	  rabbits_,
	  rabbitModels_,
	  boxes_,
	  boxModels_,
	  player_,
	  playerModel_.get(),
	  star_.get(),
	  particleSmoke_.get(),
	  particleStar_.get(),
	  [this](Rabbit* rabbit) { RemoveRabbit(rabbit); },
	  [this](size_t idx, const Vector3& offset, float r) { CreateRabbit(idx, offset, r); },
	  [this](size_t idx, const Vector3& offset, float size, float mass) { CreateBox(idx, offset, size, mass); },
	  [this](size_t idx, const Vector3& offset, float r) { PlaceStar(idx, offset, r); },
	  tpsCameraController_,
	  mainCamera_.get()
   );

   // 必要なラビット数を更新
   rabbitsRequired_ = static_cast<int>(rabbits_.size());
}

void GameScene::ClearLevel() {
   GameSceneLevelLoader::ClearLevel(
	  planets_,
	  planetModels_,
	  rabbits_,
	  rabbitModels_,
	  player_,
	  collisionManager_.get(),
	  rabbitsCollected_,
	  isStarActive_,
	  rabbitsToRemove_
   );
}

void GameScene::ExportLevelToEditor() {
   GameSceneLevelLoader::ExportLevelToEditor(
	  levelEditor_.get(),
	  planets_,
	  rabbits_,
	  star_.get(),
	  player_.get()
   );
}

void GameScene::UpdateTimer() {
   float dt = EngineContext::GetDeltaTime();
   GameSceneUpdater::UpdateTimer(dt, timerDisplay_.get());
}