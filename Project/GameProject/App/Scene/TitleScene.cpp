#include "pch.h"
#include "TitleScene.h"
#include "Utility/MathUtils.h"
#include "Framework/EngineContext.h"
#include "Object/Model/Model.h"

using namespace GameEngine;

TitleScene::TitleScene()
	: isButtonPressed_(false),
	  pressAnimationTime_(0.0f),
	  normalBlinkInterval_(0.5f),
	  fastBlinkInterval_(0.025f) {}

void TitleScene::Initialize() {
   BaseScene::Initialize();

   // フェードの初期化（黒フェード、1.5秒、EaseInOut）
   CreateDefaultFade(1.5f, 0x000000ff);

   if (sceneFade_) {
	  sceneFade_->SetEasingType(SceneFade::EasingType::EaseInOut);
	  sceneFade_->SetEasingPower(2.0f);
	  sceneFade_->StartFadeIn();
   }

   // テクスチャロード
   EngineContext::LoadTexture("resources/white1x1.png", "white1x1");
   EngineContext::LoadTexture("resources/textures/title.png", "title");
   EngineContext::LoadTexture("resources/textures/pressSpace.png", "pressSpace");

   // スカイドームのモデルとテクスチャをロード
   EngineContext::LoadModel("resources/models/skydome", "skydome.obj");
   EngineContext::LoadModel("resources/models/planet", "planet.obj");
   EngineContext::LoadTexture("resources/textures/skydome4.png", "skydome");

   // マテリアル作成
   EngineContext::CreateMaterial("TitleMaterial", 0xffffffff, 0);
   EngineContext::CreateMaterial("PressSpaceMaterial", 0xffffffff, 0);
   EngineContext::CreateMaterial("SkydomeMaterial", 0xffffffff, 0);

   // タイトルスプライト作成
   titleSprite_ = std::make_unique<Sprite>();
   titleSprite_->Create(Vector2(960.0f, 96.0f), EngineContext::GetMaterial("TitleMaterial"), Vector2(0.5f, 0.5f));
   titleSprite_->SetPosition(Vector2(0.0f, 80.0f));

   // "Press Space" スプライト作成
   pressSpaceSprite_ = std::make_unique<Sprite>();
   pressSpaceSprite_->Create(Vector2(400.0f, 48.0f), EngineContext::GetMaterial("PressSpaceMaterial"), Vector2(0.5f, 0.5f));
   pressSpaceSprite_->SetPosition(Vector2(0.0f, -160.0f));

   // スカイドームモデルの作成
   auto skydomeMat = EngineContext::GetMaterial("SkydomeMaterial");
   auto skyAsset = EngineContext::GetModel("skydome.obj");
   if (skyAsset && skydomeMat) {
	  skyDomeModel_ = std::make_unique<Model>();
	  skyDomeModel_->Create(skyAsset, skydomeMat);
   }

   auto planetAsset = EngineContext::GetModel("planet.obj");
   if (planetAsset && skydomeMat) {
	  sunModel_ = std::make_unique<Model>();
	  sunModel_->Create(planetAsset, skydomeMat);
	  sunModel_->SetScale(Vector3(5.0f, 5.0f, 5.0f));
	  sunModel_->SetPosition(Vector3(220.0f, 30.0f, 150.0f));
   }

   auto pointLight = EngineContext::GetPointLight("MainPointLight");
   if (pointLight) {
      pointLight->GetPointLightData()->intensity = 1.0f;
      pointLight->GetPointLightData()->position = Vector3(-220.0f, -30.0f, -150.0f);
      pointLight->GetPointLightData()->radius = 500.0f;
   }

   EngineContext::LoadSound("resources/sounds/bgm/title.mp3","bgmTitle");
   EngineContext::LoadSound("resources/sounds/se/button.mp3", "seButton");
   auto bgm = EngineContext::GetSound("bgmTitle");
   if (bgm) {
      bgm->Play(0.4f, true);
   }

   EngineContext::SetPostProcessEffectEnabled("Vignette", true);
   EngineContext::SetPostProcessEffectEnabled("Gauss Blur", true);
   EngineContext::SetPostProcessEffectEnabled("Bloom", true);
   EngineContext::SetPostProcessEffectEnabled("Chromatic Aberration", true);
}

void TitleScene::Update() {

   float dt = EngineContext::GetDeltaTime();
   // カメラをy軸回転させる
   Vector3 rotation = mainCamera_->GetTransform().rotation;

   mainCamera_->SetRotation(Vector3(rotation.x, rotation.y + 0.1f * dt, rotation.z));
   mainCamera_->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
   mainCamera_->Update();

   // ボタン押下アニメーション中
   if (isButtonPressed_) {
      pressAnimationTime_ += dt;
      
      // 速い点滅処理
      blinkTimer_ += dt;
      if (blinkTimer_ >= fastBlinkInterval_) {
         showPressSpace_ = !showPressSpace_;
         blinkTimer_ = 0.0f;
      }
      
      // アニメーション完了後にフェードアウトしてシーン遷移
      if (pressAnimationTime_ >= pressAnimationDuration_) {
         // フェードアウトを開始
         if (sceneFade_ && sceneFade_->GetFadeState() != SceneFade::FadeState::FadeOut) {
            sceneFade_->SetFadeColor(0x000000ff);  // 黒色フェード
            sceneFade_->SetFadeDuration(1.5f);
            sceneFade_->SetEasingType(SceneFade::EasingType::EaseInOut);
            sceneFade_->SetEasingPower(2.0f);
            sceneFade_->ResetFadeOutCompleted();
            sceneFade_->StartFadeOut();

            // フェードアウト完了後にGameシーンに遷移
            sPendingSceneName_ = "Game";
            sIsWaitingForFadeOut_ = true;

            #ifdef USE_IMGUI
            OutputDebugStringA("TitleScene: Starting fade out to Game scene\n");
            #endif
         }
      }
   } else {
      // 通常の点滅処理
      blinkTimer_ += dt;
      if (blinkTimer_ >= normalBlinkInterval_) {
         showPressSpace_ = !showPressSpace_;
         blinkTimer_ = 0.0f;
      }
      
      // スペースキーまたはAボタンでアニメーション開始
      if (EngineContext::IsKeyTriggered(KeyCode::Space) || EngineContext::IsGamePadButtonTriggered(GamePadButton::A)) {
         isButtonPressed_ = true;
         pressAnimationTime_ = 0.0f;
         blinkTimer_ = 0.0f;
         showPressSpace_ = true; // 押した瞬間は表示状態にする

		 auto seButton = EngineContext::GetSound("seButton");
         if (seButton) {
             seButton->Play(0.5f, false);
		 }

#ifdef USE_IMGUI
         OutputDebugStringA("TitleScene: Button pressed, starting animation\n");
#endif
      }
   }

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
}

void TitleScene::Draw() {
   auto texture = EngineContext::GetTexture("pressSpace");
   auto titleTexture = EngineContext::GetTexture("title");
   auto skydomeTex = EngineContext::GetTexture("skydome");
   auto sunTex = EngineContext::GetTexture("white1x1");
   EngineContext::SetActiveCamera(1); // メインカメラ

   if (sunModel_ && sunTex) {
	  EngineContext::SetBlendMode(BlendMode::kBlendModeNone);
	  EngineContext::BeginLensFlareOcclusionQuery();
	  EngineContext::Draw(sunModel_.get(), sunTex);
	  EngineContext::EndLensFlareOcclusionQuery();
   }

   // スカイドーム描画（メインカメラで）
   if (skyDomeModel_ && skydomeTex) {
	  EngineContext::SetBlendMode(BlendMode::kBlendModeNone);
	  EngineContext::Draw(skyDomeModel_.get(), skydomeTex);
   }

   // タイトルテキスト描画
   if (titleSprite_ && titleTexture) {
	  EngineContext::DrawUI(
		 titleSprite_.get(),
		 titleTexture,
		 Sprite::AnchorPoint::MiddleCenter,
		 BlendMode::kBlendModeNormal,
		 true,
		 1280,
		 720
	  );
   }

   // "Press Space" テキスト描画（点滅）
   if (showPressSpace_ && pressSpaceSprite_ && texture) {
	  EngineContext::DrawUI(
		 pressSpaceSprite_.get(),
		 texture,
		 Sprite::AnchorPoint::MiddleCenter,
		 BlendMode::kBlendModeNormal,
		 true,
		 1280,
		 720
	  );
   }

   // フェードを描画
   BaseScene::Draw();
}

void TitleScene::Finalize() {
   BaseScene::Finalize();
}
