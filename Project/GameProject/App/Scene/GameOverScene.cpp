#include "GameOverScene.h"
#include "Framework/EngineContext.h"
#include "Core/Graphics/Material.h"
#include "Utility/MathUtils/EasingFunctions.h"
#include "Object/Model/Model.h"
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui.h"
#endif

using namespace GameEngine;

// 静的メンバ変数の定義
int GameOverScene::sTipsIndex_ = 0;

GameOverScene::GameOverScene() {}

void GameOverScene::Initialize() {
   BaseScene::Initialize();

   // 現在のインスタンスで使用するTipsインデックスを取得し、次回のために更新
   currentTipsIndex_ = sTipsIndex_;
   sTipsIndex_ = (sTipsIndex_ + 1) % 3;  // 0, 1, 2 を順番に循環

   // フェードの初期化（白フェード、1.5秒、EaseInOut）
   CreateDefaultFade(1.5f, 0x000000ff);
   if (sceneFade_) {
	  sceneFade_->SetEasingType(SceneFade::EasingType::EaseInOut);
	  sceneFade_->SetEasingPower(2.0f);
   }

   // フェードインを開始
   if (sceneFade_) {
	  sceneFade_->StartFadeIn();
   }

   // テクスチャロード
   EngineContext::LoadTexture("resources/textures/gameOver.png", "gameOver");
   EngineContext::LoadTexture("resources/textures/restart.png", "restart");
   EngineContext::LoadTexture("resources/textures/toTitle.png", "toTitle");
   EngineContext::LoadTexture("resources/textures/tips1.png", "tips1");
   EngineContext::LoadTexture("resources/textures/tips2.png", "tips2");
   EngineContext::LoadTexture("resources/textures/tips3.png", "tips3");

   // キーコンフィグの初期化
   keyConfig_ = std::make_unique<KeyConfig>();

   // キーボード設定
   keyConfig_->BindKey("MoveLeft", KeyCode::A);
   keyConfig_->BindKey("MoveRight", KeyCode::D);
   keyConfig_->BindKey("Decide", KeyCode::Space);

   // ゲームパッド設定
   keyConfig_->BindGamePad("Decide", GamePadButton::A);
   keyConfig_->BindLeftStick("Move");

   // スプライトの作成
   auto spriteMat = EngineContext::GetMaterial("Sprite");
   if (!spriteMat) {
	  EngineContext::CreateMaterial("Sprite", 0xffffffff, 0);
	  spriteMat = EngineContext::GetMaterial("Sprite");
   }

   // 上のスプライト（ゲームオーバーテキスト） - サイズは960x96
   topSprite_ = std::make_unique<Sprite>();
   topSprite_->Create(Vector2(640.0f, 64.0f), spriteMat, Vector2(0.5f, 0.5f));
   topSprite_->SetPosition(Vector2(0.0f, 200.0f));  // 画面上部
   topSprite_->SetScale(Vector2(1.2f, 1.2f));

   // 左下のスプライト（リトライ） - サイズは400x48
   leftSprite_ = std::make_unique<Sprite>();
   leftSprite_->Create(Vector2(360.0f, 64.0f), spriteMat, Vector2(0.5f, 0.5f));
   leftSprite_->SetPosition(Vector2(-200.0f, -200.0f));  // 左下

   // 右下のスプライト（タイトルへ） - サイズは400x48
   rightSprite_ = std::make_unique<Sprite>();
   rightSprite_->Create(Vector2(360.0f, 64.0f), spriteMat, Vector2(0.5f, 0.5f));
   rightSprite_->SetPosition(Vector2(200.0f, -200.0f));  // 右下

   // チップススプライト(中央下)
   tipsSprite_ = std::make_unique<Sprite>();
   tipsSprite_->Create(Vector2(640.0f, 64.0f), spriteMat, Vector2(0.5f, 0.5f));
   tipsSprite_->SetPosition(Vector2(0.0f, 25.0f));  // 中央下
   tipsSprite_->SetScale(Vector2(1.5f, 1.5f));

   // スカイドームとサンのロード
   EngineContext::LoadModel("resources/models/skydome", "skydome.obj");
   EngineContext::LoadModel("resources/models/planet", "planet.obj");
   EngineContext::LoadTexture("resources/textures/skydome4.png", "skydome");

   // マテリアル作成
   EngineContext::CreateMaterial("SkydomeMaterial", 0xffffffff, 0);

   // スカイドームモデルの作成
   auto skydomeMat = EngineContext::GetMaterial("SkydomeMaterial");
   auto skyAsset = EngineContext::GetModel("skydome.obj");
   if (skyAsset && skydomeMat) {
	  skyDomeModel_ = std::make_unique<Model>();
	  skyDomeModel_->Create(skyAsset, skydomeMat);
   }

   // サンモデルの作成
   auto planetAsset = EngineContext::GetModel("planet.obj");
   if (planetAsset && skydomeMat) {
	  sunModel_ = std::make_unique<Model>();
	  sunModel_->Create(planetAsset, skydomeMat);
	  sunModel_->SetScale(Vector3(5.0f, 5.0f, 5.0f));
	  sunModel_->SetPosition(Vector3(220.0f, 30.0f, 150.0f));
   }

   // ポイントライトの設定
   auto pointLight = EngineContext::GetPointLight("MainPointLight");
   if (pointLight) {
      pointLight->GetPointLightData()->intensity = 1.0f;
      pointLight->GetPointLightData()->position = Vector3(220.0f, 30.0f, 150.0f);
      pointLight->GetPointLightData()->radius = 500.0f;
   }

   // カメラの初期位置
   mainCamera_->SetPosition(Vector3(0.0f, 0.0f, 0.0f));

   elapsedTime_ = 0.0f;
   animationTime_ = 0.0f;
   hasRequestedTransition_ = false;
   currentSelection_ = SelectionState::Left;  // デフォルトは左（リトライ）
   isPressingButton_ = false;
   pressAnimationTime_ = 0.0f;

   EngineContext::SetPostProcessEffectEnabled("Chromatic Aberration", true);

#ifdef USE_IMGUI
   OutputDebugStringA("=== GameOverScene Initialized ===\n");
#endif

   EngineContext::SetPostProcessEffectEnabled("Vignette", true);
   EngineContext::SetPostProcessEffectEnabled("Gauss Blur", true);
   EngineContext::SetPostProcessEffectEnabled("Bloom", true);

   EngineContext::LoadSound("resources/sounds/bgm/title.mp3", "bgmTitle");
   EngineContext::LoadSound("resources/sounds/se/button.mp3", "seButton");
   EngineContext::LoadSound("resources/sounds/se/gameOver.mp3", "seGameOver");
   auto bgm = EngineContext::GetSound("bgmStage");
   if (bgm) {
      bgm->Stop();
   }

   bgm = EngineContext::GetSound("bgmTitle");
   if (bgm) {
      bgm->Play(0.2f, true);
   }

   auto seGameOver = EngineContext::GetSound("seGameOver");
   if (seGameOver) {
      seGameOver->Play(0.3f, false);
   }
}

void GameOverScene::Update() {
   // フェードの更新を先に行う
   if (sceneFade_) {
      sceneFade_->Update();

      // フェードアウトが完了したらシーン切り替えフラグを立てる
      if (sIsWaitingForFadeOut_ && sceneFade_->IsFadeOutCompleted()) {
         sNextSceneName_ = sPendingSceneName_;
         sIsWaitingForFadeOut_ = false;
         sPendingSceneName_ = "";
      }
   }

   float dt = EngineContext::GetDeltaTime();
   elapsedTime_ += dt;
   
   // カメラをy軸回転させる
   Vector3 rotation = mainCamera_->GetTransform().rotation;
   mainCamera_->SetRotation(Vector3(rotation.x, rotation.y + cameraRotationSpeed_ * dt, rotation.z));
   mainCamera_->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
   BaseScene::Update();
   
   // ボタン押下アニメーション中
   if (isPressingButton_) {
      pressAnimationTime_ += dt;
      
      // スケール更新
      UpdateSpriteScales();
      
      // アニメーション完了後にフェードアウト開始
      if (pressAnimationTime_ >= pressAnimationDuration_) {
         isPressingButton_ = false;

         // フェードアウトしてシーン遷移
         std::string nextScene;
         if (currentSelection_ == SelectionState::Left) {
            nextScene = "Game";  // ゲームシーンに戻る（リトライ）
         } else {
            nextScene = "Title";  // タイトルシーンに戻る
         }

         if (sceneFade_) {
            sceneFade_->SetFadeColor(0x000000ff);  // 白色フェード
            sceneFade_->SetFadeDuration(1.5f);
            sceneFade_->SetEasingType(SceneFade::EasingType::EaseInOut);
            sceneFade_->SetEasingPower(2.0f);
            sceneFade_->ResetFadeOutCompleted();
            sceneFade_->StartFadeOut();

            // フェードアウト完了後にシーン遷移
            sPendingSceneName_ = nextScene;
            sIsWaitingForFadeOut_ = true;

#ifdef USE_IMGUI
            char debugBuffer[256];
            sprintf_s(debugBuffer, "GameOverScene: Starting fade out to %s\n", nextScene.c_str());
            OutputDebugStringA(debugBuffer);
#endif
         } else {
            // フェードがない場合は直接遷移
            sNextSceneName_ = nextScene;
         }
      }
   } else {
      animationTime_ += dt;
   }

   // 選択の更新とスケイル更新（アニメーション中でない場合のみ）
   if (!hasRequestedTransition_ && !isPressingButton_) {
      UpdateSelection();
      UpdateSpriteScales();
   }

#ifdef USE_IMGUI
   // デバッグ情報表示
   ImGui::Begin("GameOver Scene Debug");
   ImGui::Text("Elapsed Time: %.2f", elapsedTime_);
   ImGui::Text("Animation Time: %.2f", animationTime_);
   ImGui::Text("Current Selection: %s", currentSelection_ == SelectionState::Left ? "LEFT (Retry)" : "RIGHT (Title)");
   ImGui::Text("Transition Requested: %s", hasRequestedTransition_ ? "YES" : "NO");
   ImGui::Text("Pressing Button: %s", isPressingButton_ ? "YES" : "NO");
   ImGui::Text("Press Animation Time: %.2f", pressAnimationTime_);
   if (sceneFade_) {
      const char* fadeStateStr = "None";
      auto fadeState = sceneFade_->GetFadeState();
      if (fadeState == SceneFade::FadeState::FadeIn) fadeStateStr = "FadeIn";
      else if (fadeState == SceneFade::FadeState::FadeOut) fadeStateStr = "FadeOut";
      ImGui::Text("Fade State: %s", fadeStateStr);
   }
   ImGui::End();
#endif
}

void GameOverScene::UpdateSelection() {
   // キーボード入力
   if (keyConfig_->IsTriggered("MoveLeft")) {
	  currentSelection_ = SelectionState::Left;
   }
   if (keyConfig_->IsTriggered("MoveRight")) {
	  currentSelection_ = SelectionState::Right;
   }

   // ゲームパッドのスティック入力
   Vector2 stickInput = keyConfig_->GetStickVector("Move", 0, 0.5f);
   if (stickInput.x < -0.5f) {
	  currentSelection_ = SelectionState::Left;
   } else if (stickInput.x > 0.5f) {
	  currentSelection_ = SelectionState::Right;
   }

   // 決定ボタン
   if (keyConfig_->IsTriggered("Decide")) {
	  HandleDecision();
   }
}

void GameOverScene::UpdateSpriteScales() {
   // ボタン押下アニメーション中の処理
   if (isPressingButton_) {
      // 進行度を計算
      float progress = pressAnimationTime_ / pressAnimationDuration_;
      
      // 前半で大きくなり、後半で元に戻る
      float currentScale;
      if (progress < 0.5f) {
         // 前半: 1.0 -> 1.5 (EaseOutBack)
         float halfProgress = progress * 2.0f; // 0.0 -> 1.0にマップ
         currentScale = Easing::EaseOutBack(1.0f, pressScaleMultiplier_, halfProgress);
      } else {
         // 後半: 1.5 -> 1.0 (EaseIn)
         float halfProgress = (progress - 0.5f) * 2.0f; // 0.0 -> 1.0にマップ
         currentScale = Easing::EaseIn(pressScaleMultiplier_, 1.0f, halfProgress, 2.0f);
      }
      
      // 選択されたボタンのみスケールを変更
      if (currentSelection_ == SelectionState::Left) {
         leftSprite_->SetScale(Vector2(currentScale, currentScale));
         rightSprite_->SetScale(Vector2(unselectedScale_, unselectedScale_));
      } else {
         rightSprite_->SetScale(Vector2(currentScale, currentScale));
         leftSprite_->SetScale(Vector2(unselectedScale_, unselectedScale_));
      }
   } else {
      // 通常のアニメーション（sin波で1.0f～1.1fを行き来）
      float sinWave = std::sin(animationTime_ * scaleFrequency_);
      float scaleFactor = minSelectedScale_ + (sinWave * 0.5f + 0.5f) * (maxSelectedScale_ - minSelectedScale_);

      // 左のスプライト
      if (currentSelection_ == SelectionState::Left) {
         leftSprite_->SetScale(Vector2(scaleFactor, scaleFactor));
      } else {
         leftSprite_->SetScale(Vector2(unselectedScale_, unselectedScale_));
      }

      // 右のスプライト
      if (currentSelection_ == SelectionState::Right) {
         rightSprite_->SetScale(Vector2(scaleFactor, scaleFactor));
      } else {
         rightSprite_->SetScale(Vector2(unselectedScale_, unselectedScale_));
      }
   }

   // 上のスプライトは常に固定スケール
   topSprite_->SetScale(Vector2(1.2f, 1.2f));
}

void GameOverScene::HandleDecision() {
   if (hasRequestedTransition_ || isPressingButton_) {
	  return;
   }

   auto seButton = EngineContext::GetSound("seButton");
   if (seButton) {
      seButton->Play(0.5f, false);
   }

   hasRequestedTransition_ = true;
   isPressingButton_ = true;
   pressAnimationTime_ = 0.0f;

#ifdef USE_IMGUI
   char debugBuffer[256];
   sprintf_s(debugBuffer, "GameOverScene: Button pressed - %s\n",
	  currentSelection_ == SelectionState::Left ? "Retry" : "Title");
   OutputDebugStringA(debugBuffer);
#endif
}

void GameOverScene::Draw() {
   // スカイドームとサンを描画
   auto skydomeTex = EngineContext::GetTexture("skydome");
   auto sunTex = EngineContext::GetTexture("white1x1");
   EngineContext::SetActiveCamera(1); // メインカメラ
   
   // サン描画（レンズフレア用）
   if (sunModel_ && sunTex) {
      EngineContext::SetBlendMode(BlendMode::kBlendModeNone);
      EngineContext::BeginLensFlareOcclusionQuery();
      EngineContext::Draw(sunModel_.get(), sunTex);
      EngineContext::EndLensFlareOcclusionQuery();
   }
   
   // スカイドーム描画
   if (skyDomeModel_ && skydomeTex) {
      EngineContext::SetBlendMode(BlendMode::kBlendModeNone);
      EngineContext::Draw(skyDomeModel_.get(), skydomeTex);
   }
   
   // スプライトを描画（正しいテクスチャを使用）
   auto gameoverTex = EngineContext::GetTexture("gameOver");
   auto retryTex = EngineContext::GetTexture("restart");
   auto toTitleTex = EngineContext::GetTexture("toTitle");
   
   // 上のスプライト（ゲームオーバーテキスト）
   if (topSprite_ && gameoverTex) {
      EngineContext::DrawUI(topSprite_.get(), gameoverTex, Sprite::AnchorPoint::MiddleCenter);
   }
   
   // 左下のスプライト（リトライ）
   if (leftSprite_ && retryTex) {
      EngineContext::DrawUI(leftSprite_.get(), retryTex, Sprite::AnchorPoint::MiddleCenter);
   }
   
   // 右下のスプライト（タイトルへ）
   if (rightSprite_ && toTitleTex) {
      EngineContext::DrawUI(rightSprite_.get(), toTitleTex, Sprite::AnchorPoint::MiddleCenter);
   }
   
   // Tipsスプライト（現在のインデックスに応じたテクスチャを使用）
   if (tipsSprite_) {
      std::string tipsTextureName = "tips" + std::to_string(currentTipsIndex_ + 1);
      auto tipsTex = EngineContext::GetTexture(tipsTextureName);
      if (tipsTex) {
         EngineContext::DrawUI(tipsSprite_.get(), tipsTex, Sprite::AnchorPoint::MiddleCenter);
      }
   }
   
   // フェードを描画
   BaseScene::Draw();
}

void GameOverScene::Finalize() {
#ifdef USE_IMGUI
   OutputDebugStringA("=== GameOverScene Finalized ===\n");
#endif

   BaseScene::Finalize();
}
