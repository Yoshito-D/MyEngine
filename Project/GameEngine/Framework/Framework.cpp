#include "pch.h"
#include "Framework.h"
#include "ExportDump.h"
#include "D3DResourceLeakChecker.h"
#include "Utility/Logger.h"
#include "Object.h"
#include "Camera.h"
#include "Sound.h"
#include "DirectionalLight.h"
#include "SpotLight.h"
#include "ParticleSystem.h"
#include "Model/Model.h"
#include "Sprite/Sprite.h"
#include "LightDataBuffer.h"
#include "Component/ComponentRegistry.h"

namespace GameEngine {

void Framework::Initialize() {
   // 例外発生時にダンプを出力するハンドラを設定
   SetUnhandledExceptionFilter(ExportDump);

   // ロガーの初期化
   Logger::Initialize();

   // ウィンドウの初期化
   window_ = std::make_unique<Window>();
   window_->CreateGameWindow(L"LE3A_20_ヨシト_ダイキ", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

   // デバイスの初期化
   device_ = std::make_unique<GraphicsDevice>();
   device_->Initialize(window_.get());

   // GraphicsDeviceをCameraに渡す
   Camera::InitializeGraphicsDevice(device_.get());

   // GraphicsDeviceをParticleSystemに渡す
   ParticleSystem::Initialize(device_.get());

   // GraphicsDeviceをMeshに渡す
   Mesh::Initialize(device_.get());

   // GraphicsDeviceをMaterialに渡す
   Material::Initialize(device_.get());

   // GraphicsDeviceをLightに渡す
   DirectionalLight::Initialize(device_.get());
   PointLight::Initialize(device_.get());
   SpotLight::Initialize(device_.get());
   AreaLight::Initialize(device_.get());

   // GraphicsDeviceをLightDataBufferに渡す
   LightDataBuffer::Initialize(device_.get());

   // GraphicsDeviceをTransformationMatrixに渡す
   TransformationMatrix::Initialize(device_.get());

   // オーディオシステムの初期化（アセットマネージャーより前に初期化）
   audio_ = std::make_unique<Audio>();
   audio_->Initialize();

   // SoundクラスにXAudio2インターフェイスを渡す
   Sound::Initialize(audio_->GetXAudio2());

   // アセットマネージャーの初期化（オーディオシステムを渡す）
   assetManager_ = std::make_unique<AssetManager>();
   assetManager_->Initialize(device_.get(), audio_.get());
   
   // CameraManagerとLightManagerのインスタンスを生成
   cameraManager_ = std::make_unique<CameraManager>();
   lightManager_ = std::make_unique<LightManager>();

   // 描画システムの初期化 (カメラ/ライトマネージャ、アセットマネージャを渡す)
   renderer_ = std::make_unique<Renderer>();
   renderer_->Initialize(device_.get(), window_.get(), cameraManager_.get(), lightManager_.get(), assetManager_.get());

   // cameraManagerとlightManagerの初期化
   lightManager_->Initialize();

   // 入力システムの初期化
   input_ = std::make_unique<Input>();
   input_->Initialize(window_->GetInstance(), window_->GetHwnd());

   // タイムプロファイラーの初期化
   timeProfiler_ = std::make_unique<TimeProfiler>();

   // EngineContextの初期化
   std::unique_ptr<EngineContextInitializer> initializer = std::make_unique<EngineContextInitializer>();
   initializer->Initialize(
	  device_.get(),
	  input_.get(),
	  audio_.get(),
	  renderer_.get(),
	  assetManager_.get(),
	  timeProfiler_.get(),
	  cameraManager_.get(),
	  lightManager_.get()
   );
}

void Framework::BeginFrame() {
   // 入力システムの更新
   input_->Update();

   // タイムプロファイラーの更新
   timeProfiler_->Update();

   // Alt + Enter でフルスクリーン切り替え
   const bool isAltPressed = input_->IsKeyPressed(KeyCode::LeftAlt) || input_->IsKeyPressed(KeyCode::RightAlt);
   const bool isEnterTriggered = input_->IsKeyTriggered(KeyCode::Enter) || input_->IsKeyTriggered(KeyCode::NumpadEnter);

   if (isAltPressed && isEnterTriggered) {
	  device_->ToggleFullscreen();
   }

   // バックバッファサイズをウィンドウサイズに同期
   device_->SyncBackBufferSizeToWindow();
   // レンダーターゲットサイズをデバイスに同期
   renderer_->SyncRenderTargetSizeToDevice();
   // フレーム開始処理
   renderer_->BeginFrame();
}

void Framework::EndFrame() {
   // フレーム終了処理
   renderer_->EndFrame();
   // 中間リソースの解放
   assetManager_->GetTextureManager()->ReleaseIntermediateResources();
}

void Framework::Update() {
   // デルタタイムを取得
   const float deltaTime = EngineContext::GetDeltaTime();

   // 登録されている全てのモデルのコンポーネントを更新
   for (auto* model : Model::GetRegisteredModels()) {
	  if (!model) { continue; }
	  model->UpdateComponents(deltaTime);
   }

   // 登録されている全てのスプライトのコンポーネントを更新
   for (auto* sprite : Sprite::GetRegisteredSprites()) {
	  if (!sprite) { continue; }
	  sprite->UpdateComponents(deltaTime);
   }

   // 登録されている全てのパーティクルシステムを更新
   for (auto* particleSystem : ParticleSystem::GetRegisteredParticleSystems()) {
	  if (!particleSystem) { continue; }
	  particleSystem->Update(deltaTime);
	  particleSystem->UpdateMatrix(cameraManager_->GetActiveCamera());
   }
}

void Framework::Draw() {

}

void Framework::Finalize() {
   if (renderer_) {
	  renderer_->Finalize();
	  renderer_.reset();
   }

   if (assetManager_) {
	  assetManager_.reset();
   }

   if (device_) {
	  device_->Finalize();
	  device_.reset();
   }

   if (audio_) {
	  audio_->Finalize();
	  audio_.reset();
   }

   if (window_) {
	  window_->DestroyGameWindow();
	  window_.reset();
   }
}

void Framework::Run() {
   // 初期化
   Initialize();

   // メインループ
   while (window_->ProcessMessage() == 0) {
	  // フレーム開始処理
	  BeginFrame();

	  // 更新処理
	  Update();

	  // 描画処理
	  Draw();

	  // フレーム終了処理
	  EndFrame();
   }

   // 終了処理
   Finalize();
}
}
