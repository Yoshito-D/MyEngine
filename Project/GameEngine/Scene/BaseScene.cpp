#include <pch.h>
#include <BaseScene.h>
#include <EngineContext.h>
#include <ParticleSystem.h>
#include <Skybox/Skybox.h>
#include <filesystem>
#include <fstream>

namespace GameEngine {
void BaseScene::Initialize() {
   // 現在のシーンインスタンスを登録
   sCurrentScene_ = this;

   // デフォルトのライトを作成（LightManagerが所有）
   EngineContext::CreateDirectionalLight("MainDirectionalLight", 0xffffffff, Vector3(0.0f, -1.0f, 0.0f), 2.5f);
   EngineContext::CreatePointLight("MainPointLight", 0xffffffff, Vector3(0.0f, 0.0f, 0.0f), 0.0f);
   EngineContext::CreateSpotLight("MainSpotLight", 0xffffffff, Vector3(), 0.0f, Vector3(0.0f, -1.0f, 0.0f), 5.0f, 0.1f, 0.7f, 0.9f);
   EngineContext::CreateAreaLight("MainAreaLight", Vector3(0.0f, 10.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector2(5.0f, 5.0f), Vector3(1.0f, 1.0f, 1.0f), 0.0f);

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
   LoadDebugCameraState();
   unit->brain->RegisterVirtualCamera(debugCamera_.get());
   unit->brain->SetDefaultBlendTime(0.0f);

   cameraEditor_ = std::make_unique<CameraEditor>();
   cameraEditor_->Initialize(EngineContext::GetLineRenderer());
   cameraEditor_->SetTargetBrain(unit->brain.get());

   editorSceneContext_ = std::make_unique<EditorSceneContext>();
   editorSceneContext_->Initialize(editorSceneName_);
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
}

void BaseScene::Finalize() {
   // 現在のシーンインスタンスをクリア
   if (sCurrentScene_ == this) {
	  sCurrentScene_ = nullptr;
   }

#ifdef USE_IMGUI
   SaveDebugCameraState();
#endif

   // アクティブなBrainに登録されている全VirtualCameraを解除してからCameraUnitを破棄
   if (auto* brain = EngineContext::GetActiveBrain()) {
	  auto vcams = brain->GetVirtualCameras(); // コピーして反復
	  for (auto* vcam : vcams) {
		 brain->UnregisterVirtualCamera(vcam);
	  }
   }

#ifdef USE_IMGUI
   // デバッグカメラを先に破棄（Brain破棄より前に行う）
   editorSceneContext_.reset();
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
   sNextSceneName_ = sceneName;
}

#ifdef USE_IMGUI
void BaseScene::LoadEditorSceneIfNeeded() {
   if (editorSceneContext_) {
	  editorSceneContext_->AutoLoad();
   }
}

std::filesystem::path BaseScene::GetDebugCameraStateFilePath() const {
   return std::filesystem::path("resources") / "editor" / "debug_cameras" / (editorSceneName_ + ".json");
}

void BaseScene::LoadDebugCameraState() {
   if (!debugCamera_) {
	  return;
   }

   const std::filesystem::path filePath = GetDebugCameraStateFilePath();
   if (!std::filesystem::exists(filePath)) {
	  return;
   }

   std::ifstream file(filePath);
   if (!file.is_open()) {
	  return;
   }

   nlohmann::json root;
   try {
	  file >> root;
   }
   catch (...) {
	  return;
   }

   const nlohmann::json* cameraData = nullptr;
   if (root.is_object() && root.contains("debugCamera") && root.at("debugCamera").is_object()) {
	  cameraData = &root.at("debugCamera");
   } else if (root.is_object()) {
	  cameraData = &root;
   }
   if (!cameraData) {
	  return;
   }

   const int priority = debugCamera_->GetPriority();
   const bool active = debugCamera_->IsActive();
   debugCamera_->Deserialize(*cameraData);
   debugCamera_->SetName("DebugCamera");
   debugCamera_->SetPriority(priority);
   debugCamera_->SetActive(active);
}

void BaseScene::SaveDebugCameraState() const {
   if (!debugCamera_) {
	  return;
   }

   const std::filesystem::path filePath = GetDebugCameraStateFilePath();
   std::error_code error;
   std::filesystem::create_directories(filePath.parent_path(), error);
   if (error) {
	  return;
   }

   std::ofstream file(filePath);
   if (!file.is_open()) {
	  return;
   }

   nlohmann::json root = nlohmann::json::object();
   root["version"] = 1;
   root["sceneName"] = editorSceneName_;
   root["debugCamera"] = debugCamera_->Serialize();
   file << root.dump(3);
}
#endif

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
