#include <pch.h>
#include <BaseScene.h>
#include <EngineContext.h>
#include <ParticleSystem.h>
#include <Skybox/Skybox.h>

namespace GameEngine {
void BaseScene::Initialize() {
   // 現在のシーンインスタンスを登録
   sCurrentScene_ = this;

   // デフォルトのライトを作成（LightManagerが所有）
   EngineContext::LoadTexture("resources/textures/white1x1.png", "white1x1");
   EngineContext::LoadTexture("resources/textures/uvChecker.png", "uvChecker");
   EngineContext::CreateDirectionalLight("MainDirectionalLight");
   EngineContext::CreatePointLight("MainPointLight", 0xffffffff, Vector3(0.0f, 0.0f, 0.0f), 0.0f);
   EngineContext::CreateSpotLight("MainSpotLight", 0xffffffff, Vector3(), 0.0f, Vector3(0.0f, -1.0f, 0.0f), 5.0f, 0.1f, 0.7f, 0.9f);
   EngineContext::CreateAreaLight("MainAreaLight", 
      Vector3(0.0f, 10.0f, 0.0f),         
      Vector3(0.0f, -1.0f, 0.0f),         
      Vector3(1.0f, 0.0f, 0.0f),          
      Vector2(5.0f, 5.0f),                
      Vector3(1.0f, 1.0f, 1.0f),          
      0.0f                                
   );

   // CameraUnitを生成（Brain+Cameraのペア）
   CameraUnit* unit = EngineContext::CreateCameraUnit();

   auto mainCamera = std::make_unique<Camera>();
   mainCamera->Initialize();
   mainCamera->SetFarClip(10000.0f);
   unit->brain->Initialize(std::move(mainCamera));

#ifdef USE_IMGUI
   debugCamera_ = std::make_unique<DebugCamera>();
   debugCamera_->Initialize();
   debugCamera_->SetPriority(-1); // 通常時は選ばれない
   unit->brain->RegisterVirtualCamera(debugCamera_.get());
   unit->brain->SetDefaultBlendTime(0.0f);

   cameraEditor_ = std::make_unique<CameraEditor>();
   cameraEditor_->Initialize(EngineContext::GetLineRenderer());
   cameraEditor_->SetTargetBrain(unit->brain.get());
#endif
}

void BaseScene::Update() {
#ifdef USE_IMGUI
   if (EngineContext::IsKeyTriggered(KeyCode::F1)) {
      isDebugCameraActive_ = !isDebugCameraActive_;
      if (isDebugCameraActive_) {
         // 最高優先度を与えてDebugCameraを選択させる
         debugCamera_->SetPriority(100);
      } else {
         // 優先度を戻してゲーム用VirtualCameraに戻す
         debugCamera_->SetPriority(-1);
      }
   }

   // DebugCamera使用中かどうかに関わらず、常にBrain経由で更新する
   // （Brain内でアクティブなVirtualCameraが自動選択される）
   {
      float deltaTime = EngineContext::GetDeltaTime();
      EngineContext::GetActiveBrain()->Update(deltaTime);
   }

   EngineContext::DebugDrawLights();
#else
   {
      float deltaTime = EngineContext::GetDeltaTime();
      EngineContext::GetActiveBrain()->Update(deltaTime);
   }
#endif // _DEBUG


   // フェードの更新（デルタタイムは内部で取得）
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

void BaseScene::Draw() {
#ifdef USE_IMGUI
   // カメラエディタウィンドウを表示
   if (cameraEditor_) {
      cameraEditor_->ShowEditorWindow();
      if (isDebugCameraActive_) {
         cameraEditor_->DrawGizmos(EngineContext::GetActiveBrain()->GetOutputCamera());
      }
   }
#endif

   // フェードを描画
   if (sceneFade_) {
      sceneFade_->Draw();
   }
}

void BaseScene::Finalize() {
   // 現在のシーンインスタンスをクリア
   if (sCurrentScene_ == this) {
      sCurrentScene_ = nullptr;
   }

   // アクティブなBrainに登録されている全VirtualCameraを解除してからCameraUnitを破棄
   if (auto* brain = EngineContext::GetActiveBrain()) {
      auto vcams = brain->GetVirtualCameras(); // コピーして反復
      for (auto* vcam : vcams) {
         brain->UnregisterVirtualCamera(vcam);
      }
   }

#ifdef USE_IMGUI
   // デバッグカメラを先に破棄（Brain破棄より前に行う）
   debugCamera_.reset();
   cameraEditor_.reset();
#endif

   EngineContext::ClearCameraUnits();
   EngineContext::ClearDirectionalLights();
   EngineContext::ClearPointLights();
   EngineContext::ClearSpotLights();
   EngineContext::ClearAreaLights();

   Model::ClearRegisteredModels();
   Sprite::ClearRegisteredSprites();
   ParticleSystem::ClearRegisteredParticleSystems();
   Skybox::ClearRegisteredSkyboxes();
   sNextSceneName_ = "";
   sIsWaitingForFadeOut_ = false;
   sPendingSceneName_ = "";
}

void BaseScene::SetNextSceneName(const std::string& sceneName) {

   // 現在のシーンインスタンスが存在し、フェードが有効な場合はフェードアウトを開始
   if (sCurrentScene_ && sCurrentScene_->sceneFade_ && !sIsWaitingForFadeOut_) {
	  sPendingSceneName_ = sceneName;
	  sIsWaitingForFadeOut_ = true;

	  // フェードの設定を1.5秒、EaseInOutに統一
	  sCurrentScene_->sceneFade_->SetFadeDuration(1.5f);
	  sCurrentScene_->sceneFade_->SetEasingType(SceneFade::EasingType::EaseInOut);
	  sCurrentScene_->sceneFade_->SetEasingPower(2.0f);
	  sCurrentScene_->sceneFade_->ResetFadeOutCompleted();
	  sCurrentScene_->sceneFade_->StartFadeOut();
   } else {
	  // フェードが無効な場合は直接設定
	  sNextSceneName_ = sceneName;
   }
}

void BaseScene::SetFade(std::unique_ptr<SceneFade> fade) {
   sceneFade_ = std::move(fade);
}

void BaseScene::CreateDefaultFade(float fadeDuration, uint32_t fadeColor) {
   sceneFade_ = std::make_unique<SceneFade>();
   sceneFade_->Initialize(fadeDuration, fadeColor);
   sceneFade_->SetEasingType(SceneFade::EasingType::EaseInOut);
   sceneFade_->SetEasingPower(2.0f);
   sceneFade_->StartFadeIn();
}

void BaseScene::UpdateDebugCamera() {
#ifdef USE_IMGUI
   if (EngineContext::IsKeyTriggered(KeyCode::F1)) {
	  isDebugCameraActive_ = !isDebugCameraActive_;
	  if (isDebugCameraActive_) {
		 debugCamera_->SetPriority(100);
	  } else {
		 debugCamera_->SetPriority(-1);
	  }
   }

   {
	  float deltaTime = EngineContext::GetDeltaTime();
	  EngineContext::GetActiveBrain()->Update(deltaTime);
   }
#endif // USE_IMGUI
}
}
