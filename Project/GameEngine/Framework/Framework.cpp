#include "pch.h"
#include "Framework.h"
#include "ExportDump.h"
#include "D3DResourceLeakChecker.h"
#include "Utility/Logger.h"
#include "Object.h"
#include "Camera.h"
#include "Sound.h"
#include "SpotLight.h"
#include "ParticleSystem.h"
#include "LightDataBuffer.h"

namespace GameEngine {

void Framework::Initialize() {
   SetUnhandledExceptionFilter(ExportDump);

   Logger& logger = Logger::GetInstance();
   logger.Initialize();

   // ウィンドウの初期化
   window_ = std::make_unique<Window>();
   window_->CreateGameWindow(L"LE2A_20_ヨシト_ダイキ", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);

   // デバイスの初期化
   device_ = std::make_unique<GraphicsDevice>();
   device_->Initialize(window_.get());

   Object::Initialize(device_.get());

   Camera::InitializeGraphicsDevice(device_.get());

   ParticleSystem::Initialize(device_.get());

   Mesh::Initialize(device_.get());

   DirectionalLight::Initialize(device_.get());

   PointLight::Initialize(device_.get());

   SpotLight::Initialize(device_.get());

   AreaLight::Initialize(device_.get());
   
   LightDataBuffer::Initialize(device_.get());

   Material::Initialize(device_.get());

   TransformationMatrix::Initialize(device_.get());

   // 新規マネージャ生成（LightManagerの初期化は後回し）
   cameraManager_ = std::make_unique<CameraManager>();
   lightManager_  = std::make_unique<LightManager>();

   // オーディオシステムの初期化（アセットマネージャーより前に初期化）
   audio_ = std::make_unique<Audio>();
   audio_->Initialize();

   Sound::Initialize(audio_->GetXAudio2());

   // アセットマネージャーの初期化（オーディオシステムを渡す）
   assetManager_ = std::make_unique<AssetManager>();
   assetManager_->Initialize(device_.get(), audio_.get());

   // 描画システムの初期化 (カメラ/ライトマネージャ、アセットマネージャを渡す)
   renderer_ = std::make_unique<Renderer>();
   renderer_->Initialize(device_.get(), window_.get(), cameraManager_.get(), lightManager_.get(), assetManager_.get());

   // LightManagerの初期化をここで行う（テクスチャ読み込み後、Renderer初期化後）
   // これにより、SRVインデックスの衝突を回避
   lightManager_->Initialize();

   // 入力システムの初期化
   input_ = std::make_unique<Input>();
   input_->Initialize(window_->GetInstance(), window_->GetHwnd());

   // タイムプロファイラーの初期化
   timeProfiler_ = std::make_unique<TimeProfiler>();

   // JSON データマネージャーの初期化
   jsonDataManager_ = std::make_unique<JsonDataManager>();

   std::unique_ptr<EngineContextInitializer> initializer = std::make_unique<EngineContextInitializer>();
   initializer->Initialize(
      device_.get(),
      input_.get(),
      audio_.get(),
      renderer_.get(),
      assetManager_.get(),
      timeProfiler_.get(),
	  cameraManager_.get(),
	  lightManager_.get(),
	  jsonDataManager_.get()
   );
}

void Framework::BeginFrame() {
   input_->Update();
   renderer_->BeginFrame();
   timeProfiler_->Update();
}

void Framework::EndFrame() {
   renderer_->EndFrame();
   assetManager_->GetTextureManager()->ReleaseIntermediateResources();
}

void Framework::Update() {

}

void Framework::Draw() {

}

void Framework::Finalize() {
   renderer_->Finalize();
   audio_->Finalize();
   window_->DestroyGameWindow();
}

void Framework::Run() {
   Initialize();

   while (window_->ProcessMessage() == 0) {
      BeginFrame();
      Update();
      Draw();
      EndFrame();
   }

   Finalize();
}
}