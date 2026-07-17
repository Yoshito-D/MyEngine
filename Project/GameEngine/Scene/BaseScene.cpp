#include <pch.h>
#include <BaseScene.h>
#include <EngineContext.h>
#include <Editor/EditorObjectStore.h>
#include <Model/Model.h>
#include <Object.h>
#include <ParticleSystem.h>
#include <Sprite/Sprite.h>
#include <Object/Text/UIText.h>
#include <Component/RenderComponent.h>
#include <Skybox/Skybox.h>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

namespace {
std::string GetSceneObjectTypeName(const GameEngine::Object* object) {
   if (dynamic_cast<const GameEngine::UIText*>(object)) {
	  return "UIText";
   }
   if (dynamic_cast<const GameEngine::Model*>(object)) {
	  return "Model";
   }
   if (dynamic_cast<const GameEngine::Sprite*>(object)) {
	  return "Sprite";
   }
   if (dynamic_cast<const GameEngine::Skybox*>(object)) {
	  return "Skybox";
   }
   return "Object";
}

std::string BuildSceneKey(const std::string& typeName, const std::string& objectName) {
   return typeName + ":" + (objectName.empty() ? "Object" : objectName);
}

bool IsRegisteredParticleSystem(const GameEngine::ParticleSystem* particleSystem) {
   if (!particleSystem) {
	  return false;
   }

   for (auto* registered : GameEngine::ParticleSystem::GetRegisteredParticleSystems()) {
	  if (registered == particleSystem) {
		 return true;
	  }
   }
   return false;
}

bool IsEditableSceneParticleSystem(const GameEngine::ParticleSystem* particleSystem) {
   return particleSystem &&
	  particleSystem->IsEditorInspectable() &&
	  IsRegisteredParticleSystem(particleSystem);
}

class RuntimeSceneApplier {
public:
   explicit RuntimeSceneApplier(GameEngine::EditorObjectStore& objectStore)
	  : objectStore_(objectStore) {
   }

   bool Apply(const nlohmann::json& sceneData) {
	  if (!sceneData.is_object()) {
		 return false;
	  }

	  // シーン固有コードで生成されたオブジェクトは、エディタ生成物を復元する前に
	  // 固定キーへ結び付ける。これで Release でも editor_scene の上書き対象がずれない。
	  RegisterSceneOwnedKeys();
	  objectStore_.Clear();

	  if (sceneData.contains("objects") && sceneData.at("objects").is_array()) {
		 for (const auto& objectData : sceneData.at("objects")) {
			objectStore_.RestoreObject(objectData);
		 }
	  }

	  if (sceneData.contains("sceneObjects") && sceneData.at("sceneObjects").is_array()) {
		 ApplySceneObjects(sceneData.at("sceneObjects"));
	  }
	  if (sceneData.contains("sceneParticleSystems") && sceneData.at("sceneParticleSystems").is_array()) {
		 ApplySceneParticleSystems(sceneData.at("sceneParticleSystems"));
	  }
	  if (sceneData.contains("cameras") && sceneData.at("cameras").is_object()) {
		 ApplyCameras(sceneData.at("cameras"));
	  }

	  return true;
   }

private:
   void RegisterSceneOwnedKeys() {
	  std::unordered_set<std::string> usedObjectKeys;
	  for (const auto& [object, key] : sceneObjectKeys_) {
		 if (object && !key.empty()) {
			usedObjectKeys.insert(key);
		 }
	  }

	  auto registerObject = [&](GameEngine::Object* object) {
		 if (!object || objectStore_.Contains(object) || sceneObjectKeys_.contains(object)) {
			return;
		 }

		 const std::string baseKey = BuildSceneKey(GetSceneObjectTypeName(object), object->GetObjectName());
		 std::string key = baseKey;
		 int suffix = 2;
		 while (usedObjectKeys.contains(key)) {
			key = baseKey + "#" + std::to_string(suffix++);
		 }
		 usedObjectKeys.insert(key);
		 sceneObjectKeys_[object] = key;
	  };

	  for (auto* model : GameEngine::Model::GetRegisteredModels()) {
		 registerObject(model);
	  }
	  for (auto* sprite : GameEngine::Sprite::GetRegisteredSprites()) {
		 registerObject(sprite);
	  }
	  for (auto* uiText : GameEngine::UIText::GetRegisteredTexts()) {
		 registerObject(uiText);
	  }
	  for (auto* skybox : GameEngine::Skybox::GetRegisteredSkyboxes()) {
		 registerObject(skybox);
	  }

	  for (auto it = sceneParticleSystemKeys_.begin(); it != sceneParticleSystemKeys_.end();) {
		 if (!IsEditableSceneParticleSystem(it->first) || objectStore_.Contains(it->first)) {
			it = sceneParticleSystemKeys_.erase(it);
		 } else {
			++it;
		 }
	  }

	  std::unordered_set<std::string> usedParticleKeys;
	  for (const auto& [particleSystem, key] : sceneParticleSystemKeys_) {
		 if (particleSystem && !key.empty()) {
			usedParticleKeys.insert(key);
		 }
	  }

	  for (auto* particleSystem : GameEngine::ParticleSystem::GetRegisteredParticleSystems()) {
		 if (!IsEditableSceneParticleSystem(particleSystem) ||
			objectStore_.Contains(particleSystem) ||
			sceneParticleSystemKeys_.contains(particleSystem)) {
			continue;
		 }

		 const std::string baseKey = BuildSceneKey("ParticleSystem", particleSystem->GetName());
		 std::string key = baseKey;
		 int suffix = 2;
		 while (usedParticleKeys.contains(key)) {
			key = baseKey + "#" + std::to_string(suffix++);
		 }
		 usedParticleKeys.insert(key);
		 sceneParticleSystemKeys_[particleSystem] = key;
	  }
   }

   GameEngine::Object* FindSceneObjectByKey(const std::string& key) const {
	  if (key.empty()) {
		 return nullptr;
	  }

	  auto isRegistered = [](const GameEngine::Object* object) {
		 for (auto* model : GameEngine::Model::GetRegisteredModels()) {
			if (model == object) {
			   return true;
			}
		 }
		 for (auto* sprite : GameEngine::Sprite::GetRegisteredSprites()) {
			if (sprite == object) {
			   return true;
			}
		 }
		 for (auto* uiText : GameEngine::UIText::GetRegisteredTexts()) {
			if (uiText == object) {
			   return true;
			}
		 }
		 for (auto* skybox : GameEngine::Skybox::GetRegisteredSkyboxes()) {
			if (skybox == object) {
			   return true;
			}
		 }
		 return false;
	  };

	  for (const auto& [object, objectKey] : sceneObjectKeys_) {
		 if (objectKey == key && object && !objectStore_.Contains(object) && isRegistered(object)) {
			return const_cast<GameEngine::Object*>(object);
		 }
	  }
	  return nullptr;
   }

   GameEngine::ParticleSystem* FindSceneParticleSystemByKey(const std::string& key) const {
	  if (key.empty()) {
		 return nullptr;
	  }

	  for (const auto& [particleSystem, particleKey] : sceneParticleSystemKeys_) {
		 if (particleKey == key &&
			IsEditableSceneParticleSystem(particleSystem) &&
			!objectStore_.Contains(particleSystem)) {
			return const_cast<GameEngine::ParticleSystem*>(particleSystem);
		 }
	  }
	  return nullptr;
   }

   void ApplySceneObjects(const nlohmann::json& sceneObjectsData) {
	  for (const auto& entry : sceneObjectsData) {
		 if (!entry.is_object()) {
			continue;
		 }

		 const std::string key = entry.value("sceneKey", "");
		 if (key.empty()) {
			continue;
		 }

		 GameEngine::Object* object = FindSceneObjectByKey(key);
		 if (entry.value("deleted", false)) {
			if (object) {
			   if (auto* renderComponent = object->GetComponent<GameEngine::RenderComponent>()) {
				  renderComponent->visible = false;
			   }
			}
			continue;
		 }

		 if (!object) {
			continue;
		 }

		 if (auto* renderComponent = object->GetComponent<GameEngine::RenderComponent>()) {
			renderComponent->visible = true;
		 }

		 const nlohmann::json* objectData = &entry;
		 if (entry.contains("object") && entry.at("object").is_object()) {
			objectData = &entry.at("object");
		 }
		 objectStore_.ApplyObjectState(object, *objectData);
	  }
   }

   void ApplySceneParticleSystems(const nlohmann::json& sceneParticlesData) {
	  for (const auto& entry : sceneParticlesData) {
		 if (!entry.is_object()) {
			continue;
		 }

		 const std::string key = entry.value("sceneKey", "");
		 if (key.empty()) {
			continue;
		 }

		 GameEngine::ParticleSystem* particleSystem = FindSceneParticleSystemByKey(key);
		 if (entry.value("deleted", false)) {
			if (particleSystem) {
			   particleSystem->Stop();
			}
			continue;
		 }

		 if (!particleSystem) {
			continue;
		 }

		 const nlohmann::json* particleData = &entry;
		 if (entry.contains("particleSystem") && entry.at("particleSystem").is_object()) {
			particleData = &entry.at("particleSystem");
		 }
		 objectStore_.ApplyParticleSystemState(particleSystem, *particleData);
	  }
   }

   void ApplyCameras(const nlohmann::json& camerasData) {
	  if (!camerasData.is_object()) {
		 return;
	  }

	  GameEngine::CinemachineBrain* brain = GameEngine::EngineContext::GetActiveBrain();
	  if (!brain) {
		 return;
	  }

	  if (camerasData.contains("brain") && camerasData.at("brain").is_object()) {
		 const auto& brainData = camerasData.at("brain");
		 if (brainData.contains("defaultBlendTime") && brainData.at("defaultBlendTime").is_number()) {
			brain->SetDefaultBlendTime(brainData.at("defaultBlendTime").get<float>());
		 }
	  }

	  if (!camerasData.contains("virtualCameras") || !camerasData.at("virtualCameras").is_array()) {
		 return;
	  }

	  const auto& registeredCameras = brain->GetVirtualCameras();
	  std::unordered_set<GameEngine::VirtualCamera*> appliedCameras;
	  for (const auto& cameraData : camerasData.at("virtualCameras")) {
		 if (!cameraData.is_object()) {
			continue;
		 }

		 const std::string cameraName = cameraData.value("name", "");
		 if (cameraName == "DebugCamera") {
			continue;
		 }

		 GameEngine::VirtualCamera* targetCamera = nullptr;
		 if (!cameraName.empty()) {
			for (GameEngine::VirtualCamera* camera : registeredCameras) {
			   if (camera && camera->GetName() == cameraName && camera->GetName() != "DebugCamera") {
				  targetCamera = camera;
				  break;
			   }
			}
		 }

		 if (!targetCamera && cameraData.contains("index") && cameraData.at("index").is_number_unsigned()) {
			const size_t index = cameraData.at("index").get<size_t>();
			if (index < registeredCameras.size()) {
			   GameEngine::VirtualCamera* candidate = registeredCameras[index];
			   if (candidate && candidate->GetName() != "DebugCamera") {
				  targetCamera = candidate;
			   }
			}
		 }

		 if (!targetCamera || appliedCameras.contains(targetCamera)) {
			continue;
		 }

		 targetCamera->Deserialize(cameraData);
		 appliedCameras.insert(targetCamera);
	  }
   }

   GameEngine::EditorObjectStore& objectStore_;
   std::unordered_map<const GameEngine::Object*, std::string> sceneObjectKeys_;
   std::unordered_map<const GameEngine::ParticleSystem*, std::string> sceneParticleSystemKeys_;
};
} // namespace

namespace GameEngine {
BaseScene::~BaseScene() = default;

void BaseScene::Initialize() {
   // 現在のシーンインスタンスを登録
   sCurrentScene_ = this;

   // デフォルトのライトを作成（LightManagerが所有）
   EngineContext::CreateDirectionalLight("MainDirectionalLight", 0xffffffff, Vector3(0.0f, -1.0f, 0.0f), 1.0f);
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

   OnInitialize();
}

void BaseScene::Update() {
   EditorUpdate();
   RuntimeUpdate();
}

void BaseScene::EditorUpdate() {
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

   {
	  CinemachineBrain* brain = EngineContext::GetActiveBrain();
	  if (brain) {
		 if (EngineContext::ShouldRunRuntimeUpdate()) {
			brain->Update(EngineContext::GetDeltaTime());
		 } else {
			DebugCamera* editorDrivenCamera = isDebugCameraActive_ ? debugCamera_.get() : nullptr;
			brain->UpdateEditorPreview(EngineContext::GetUnscaledDeltaTime(), editorDrivenCamera);
		 }
	  }
   }

   EngineContext::DebugDrawLights();
#else
   {
	  float deltaTime = EngineContext::GetDeltaTime();
	  EngineContext::GetActiveBrain()->Update(deltaTime);
   }
#endif // _DEBUG

   OnEditorUpdate();
}

void BaseScene::RuntimeUpdate() {
   OnUpdate(EngineContext::GetDeltaTime());
}

void BaseScene::Draw() {
   OnDraw();

#ifdef USE_IMGUI
   // カメラエディタウィンドウを表示
   if (cameraEditor_) {
	  cameraEditor_->ShowEditorWindow();
	  if (CinemachineBrain* brain = EngineContext::GetActiveBrain()) {
		 cameraEditor_->DrawGizmos(brain->GetOutputCamera());
	  }
   }
#endif
}

void BaseScene::Finalize() {
   OnFinalize();

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

   if (runtimeSceneObjectStore_) {
	  runtimeSceneObjectStore_->Clear();
	  runtimeSceneObjectStore_.reset();
   }

   EngineContext::ClearCameraUnits();
   EngineContext::ClearDirectionalLights();
   EngineContext::ClearPointLights();
   EngineContext::ClearSpotLights();
   EngineContext::ClearAreaLights();

   Model::ClearRegisteredModels();
   Sprite::ClearRegisteredSprites();
   UIText::ClearRegisteredTexts();
   ParticleSystem::ClearRegisteredParticleSystems();
   Skybox::ClearRegisteredSkyboxes();
   sNextSceneName_ = "";
   sIsWaitingForFadeOut_ = false;
   sPendingSceneName_ = "";
}

void BaseScene::SetNextSceneName(const std::string& sceneName) {
   sNextSceneName_ = sceneName;
}

void BaseScene::LoadSceneDataIfNeeded() {
#ifdef USE_IMGUI
   LoadEditorSceneIfNeeded();
#else
   LoadRuntimeSceneIfNeeded();
#endif
}

void BaseScene::LoadRuntimeSceneIfNeeded() {
   const std::filesystem::path filePath = std::filesystem::path("resources") / "game" / "scenes" / (editorSceneName_ + ".json");
   if (!std::filesystem::exists(filePath)) {
	  return;
   }

   std::ifstream file(filePath);
   if (!file.is_open()) {
	  return;
   }

   nlohmann::json sceneData;
   try {
	  file >> sceneData;
   }
   catch (...) {
	  return;
   }

   runtimeSceneObjectStore_ = std::make_unique<EditorObjectStore>();
   RuntimeSceneApplier applier(*runtimeSceneObjectStore_);
   if (!applier.Apply(sceneData)) {
	  runtimeSceneObjectStore_.reset();
   }
}

#ifdef USE_IMGUI
void BaseScene::LoadEditorSceneIfNeeded() {
   if (editorSceneContext_) {
	  editorSceneContext_->AutoLoad();
   }
}

std::filesystem::path BaseScene::GetDebugCameraStateFilePath() const {
   return std::filesystem::path("resources") / "game" / "editor" / "debug_cameras" / (editorSceneName_ + ".json");
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
	  if (CinemachineBrain* brain = EngineContext::GetActiveBrain()) {
		 brain->UpdateEditorPreview(EngineContext::GetUnscaledDeltaTime(), debugCamera_.get());
	  }
   }
#endif // USE_IMGUI
}
}
