#include <pch.h>
#include <BaseScene.h>
#include <EngineContext.h>

namespace GameEngine {
void BaseScene::Initialize() {
   // 現在のシーンインスタンスを登録
   sCurrentScene_ = this;

   // デフォルトのライトを作成（LightManagerが所有）
   EngineContext::CreateDirectionalLight("MainDirectionalLight");
   EngineContext::CreatePointLight("MainPointLight", 0xffffffff, Vector3(0.0f, 0.0f, 0.0f), 0.0f);
   EngineContext::CreateSpotLight("MainSpotLight", 0xffffffff, Vector3(), 0.0f, Vector3(0.0f, -1.0f, 0.0f), 5.0f, 0.1f, 0.7f, 0.9f);
   EngineContext::CreateAreaLight("MainAreaLight", 
      Vector3(0.0f, 10.0f, 0.0f),           // position (中心座標)
      Vector3(0.0f, -1.0f, 0.0f),           // normal (照射方向: 下向き)
      Vector3(1.0f, 0.0f, 0.0f),            // tangent (右方向)
      Vector2(5.0f, 5.0f),                  // size (幅と高さ)
      Vector3(1.0f, 1.0f, 1.0f),            // color (白)
      0.0f                                  // intensity (強度)
   );

   uiCamera_ = std::make_unique<Camera>();
   uiCamera_->Initialize(Transform(), Camera::ProjectionType::Orthographic);
   uiCamera_->SetNearClip(0.1f);
   uiCamera_->SetPosition(Vector3(0.0f, 0.0f, -1.0f));
   uiCamera_->Update();
   EngineContext::AddCamera(uiCamera_.get());
   mainCamera_ = std::make_unique<Camera>();
   mainCamera_->Initialize();
   mainCamera_->SetFarClip(10000.0f);
   EngineContext::AddCamera(mainCamera_.get());
   EngineContext::SetActiveCamera(1);

#ifdef USE_IMGUI
   debugCamera_ = std::make_unique<DebugCamera>();
   debugCamera_->SetCamera(mainCamera_.get());
   mainCameraPrevTransform_ = mainCamera_->GetTransform();
#endif
}

void BaseScene::Update() {
#ifdef USE_IMGUI
   if (EngineContext::IsKeyTriggered(KeyCode::F1)) {
      isDebugCameraActive_ = !isDebugCameraActive_;
      // 切り替え時カメラの位置を保存・復元
      if (isDebugCameraActive_) {
         mainCameraPrevTransform_ = mainCamera_->GetTransform();
      } else {
         mainCamera_->SetTransform(mainCameraPrevTransform_);
      }
   }

   if (isDebugCameraActive_) {
      if (EngineContext::GetIsSceneHovered()) {
         debugCamera_->Update();
         debugCamera_->ApplyCameraTransform();
      } else {
         debugCamera_->ApplyCameraTransform();
      }
   } else {
      mainCamera_->Update();
   }
#else
   mainCamera_->Update();
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

   EngineContext::ClearCameras();
   EngineContext::ClearDirectionalLights();
   EngineContext::ClearPointLights();
   EngineContext::ClearSpotLights();
   EngineContext::ClearAreaLights();
   sNextSceneName_ = "";
   sIsWaitingForFadeOut_ = false;
   sPendingSceneName_ = "";

   // ポストプロセスを無効にする
   EngineContext::SetPostProcessEnabled(false);
}

void BaseScene::SetNextSceneName(const std::string& sceneName) {
#ifdef USE_IMGUI
   // デバッグ情報を出力
   OutputDebugStringA("=== SetNextSceneName Debug ===\n");
   char debugBuffer[256];
   sprintf_s(debugBuffer, "Scene Name: %s\n", sceneName.c_str());
   OutputDebugStringA(debugBuffer);
   sprintf_s(debugBuffer, "sCurrentScene_: %p\n", (void*)sCurrentScene_);
   OutputDebugStringA(debugBuffer);
   if (sCurrentScene_) {
	  sprintf_s(debugBuffer, "sCurrentScene_->sceneFade_: %p\n", (void*)sCurrentScene_->sceneFade_.get());
	  OutputDebugStringA(debugBuffer);
   }
   sprintf_s(debugBuffer, "sIsWaitingForFadeOut_: %s\n", sIsWaitingForFadeOut_ ? "true" : "false");
   OutputDebugStringA(debugBuffer);
#endif

   // 現在のシーンインスタンスが存在し、フェードが有効な場合はフェードアウトを開始
   if (sCurrentScene_ && sCurrentScene_->sceneFade_ && !sIsWaitingForFadeOut_) {
	  sPendingSceneName_ = sceneName;
	  sIsWaitingForFadeOut_ = true;

#ifdef USE_IMGUI
	  OutputDebugStringA("Starting fade out...\n");
#endif

	  // フェードの設定を1.5秒、EaseInOutに統一
	  sCurrentScene_->sceneFade_->SetFadeDuration(1.5f);
	  sCurrentScene_->sceneFade_->SetEasingType(SceneFade::EasingType::EaseInOut);
	  sCurrentScene_->sceneFade_->SetEasingPower(2.0f);
	  sCurrentScene_->sceneFade_->ResetFadeOutCompleted();
	  sCurrentScene_->sceneFade_->StartFadeOut();
   } else {
#ifdef USE_IMGUI
	  OutputDebugStringA("Skipping fade out - setting scene directly\n");
#endif
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
	  // 切り替え時カメラの位置を保存・復元
	  if (isDebugCameraActive_) {
		 mainCameraPrevTransform_ = mainCamera_->GetTransform();
	  } else {
		 mainCamera_->SetTransform(mainCameraPrevTransform_);
	  }
   }

   if (isDebugCameraActive_) {
	  if (EngineContext::GetIsSceneHovered()) {
		 debugCamera_->Update();
		 debugCamera_->ApplyCameraTransform();
	  } else {
		 debugCamera_->ApplyCameraTransform();
	  }
   } else {
	  mainCamera_->Update();
   }
#endif // USE_IMGUI
}
}
