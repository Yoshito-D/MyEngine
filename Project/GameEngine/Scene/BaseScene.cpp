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
#include <Component/LightComponent.h>
#include <Component/TransformComponent.h>
#include <Skybox/Skybox.h>
#include "Utility/MathUtils/QuaternionOperations.h"
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

namespace {
constexpr float kMainCameraFarClip = 10000.0f;
#ifdef USE_IMGUI
constexpr int kInactiveDebugCameraPriority = -1;
constexpr int kActiveDebugCameraPriority = 100;
constexpr int kDebugCameraStateFormatVersion = 1;
constexpr int kDebugCameraJsonIndentSize = 3;
#endif

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

void UpdateEditorLightProxies(float deltaTime) {
   const auto registeredObjects = GameEngine::Object::GetRegisteredObjects();
   for (GameEngine::Object* object : registeredObjects) {
	  if (!object) {
		 continue;
	  }
	  if (auto* light = object->GetComponent<GameEngine::LightComponent>();
		 light && light->IsEnabled()) {
		 light->Update(deltaTime);
	  }
   }
}

class RuntimeSceneApplier {
public:
   explicit RuntimeSceneApplier(GameEngine::EditorObjectStore& objectStore)
	  : objectStore_(objectStore) {}

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
	  if (sceneData.contains("environment") && sceneData.at("environment").is_object()) {
		 ApplyLegacyLights(sceneData.at("environment"));
	  }
	  if (sceneData.contains("renderSettings") &&
		 !GameEngine::EngineContext::ApplyPostProcessSceneState(sceneData.at("renderSettings"))) {
		 return false;
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

		 const std::string& entityId = object->GetEntityId();
		 const std::string baseKey = entityId.rfind("runtime_entity_", 0) == 0
			? BuildSceneKey(GetSceneObjectTypeName(object), object->GetObjectName())
			: entityId;
		 std::string key = baseKey;
		 int suffix = 2;
		 while (usedObjectKeys.contains(key)) {
			key = baseKey + "#" + std::to_string(suffix++);
		 }
		 usedObjectKeys.insert(key);
		 if (object->GetEntityId().rfind("runtime_entity_", 0) == 0) {
			object->SetEntityId(key);
		 }
		 sceneObjectKeys_[object] = key;
		 };

	  for (auto* object : GameEngine::Object::GetRegisteredObjects()) {
		 registerObject(object);
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
		 const auto& registeredObjects = GameEngine::Object::GetRegisteredObjects();
		 return std::find(registeredObjects.begin(), registeredObjects.end(), object) != registeredObjects.end();
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
			   for (const auto& component : object->GetComponentContainer().GetAll()) {
				  if (component) {
					 component->SetEnabled(false);
				  }
			   }
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

   void ApplyLegacyLights(const nlohmann::json& environment) {
	  if (!environment.contains("lights") || !environment.at("lights").is_array()) {
		 return;
	  }

	  for (const auto& lightData : environment.at("lights")) {
		 if (!lightData.is_object()) {
			continue;
		 }
		 const std::string id = lightData.value("id", "");
		 if (id.empty()) {
			continue;
		 }

		 GameEngine::Object* entity = GameEngine::Object::FindByEntityId(id);
		 if (!entity) {
			entity = objectStore_.CreateGenericObject(nullptr, id);
			if (entity) {
			   entity->SetObjectName(id);
			}
		 }
		 if (!entity) {
			continue;
		 }

		 auto* light = entity->GetComponent<GameEngine::LightComponent>();
		 if (!light) {
			light = entity->AddComponent<GameEngine::LightComponent>();
		 }
		 if (light) {
			light->DeserializeLegacy(lightData);
		 }
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

   auto createDefaultLight = [this](
	  const char* entityId,
	  LightComponent::Type type,
	  const Vector3& position,
	  const Vector3& direction,
	  float intensity) {
		 auto entity = std::make_unique<Object>();
		 entity->SetEntityId(entityId);
		 entity->SetObjectName(entityId);
		 auto* transform = entity->AddComponent<TransformComponent>();
		 transform->transform.translation = position;
		 if (type != LightComponent::Type::Point) {
			transform->transform.SetRotationQuaternion(
			   LookRotation(direction, Vector3(0.0f, 1.0f, 0.0f)));
		 }
		 auto* light = entity->AddComponent<LightComponent>();
		 light->SetLightType(type);
		 light->intensity = intensity;
		 sceneEntities_.push_back(std::move(entity));
	  };

   // デフォルトライトも通常Entityとして所有し、ヒエラルキー・保存・親子Transformを共通化する。
   createDefaultLight("MainDirectionalLight", LightComponent::Type::Directional,
	  Vector3(), Vector3(0.0f, -1.0f, 0.0f), 1.0f);
   createDefaultLight("MainPointLight", LightComponent::Type::Point,
	  Vector3(), Vector3(0.0f, -1.0f, 0.0f), 0.0f);
   createDefaultLight("MainSpotLight", LightComponent::Type::Spot,
	  Vector3(), Vector3(0.0f, -1.0f, 0.0f), 0.0f);
   createDefaultLight("MainAreaLight", LightComponent::Type::Area,
	  Vector3(0.0f, 10.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f), 0.0f);

   // CameraUnitを生成（Brain+Cameraのペア）
   CameraUnit* unit = EngineContext::CreateCameraUnit();

   auto mainCamera = std::make_unique<Camera>();
   mainCamera->Initialize();
   mainCamera->SetFarClip(kMainCameraFarClip);
   unit->brain->Initialize(std::move(mainCamera));

#ifdef USE_IMGUI
   debugCamera_ = std::make_unique<DebugCamera>();
   debugCamera_->Initialize();
   debugCamera_->SetPriority(kInactiveDebugCameraPriority); // 通常時は選ばれない
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
   if (!EngineContext::ShouldRunRuntimeUpdate()) {
	  // 編集停止中は描画プロキシだけを同期し、ゲームプレイ用Componentを誤って進めない。
	  UpdateEditorLightProxies(EngineContext::GetUnscaledDeltaTime());
   }

   if (EngineContext::IsKeyTriggered(KeyCode::F1)) {
	  isDebugCameraActive_ = !isDebugCameraActive_;
	  if (isDebugCameraActive_) {
		 // 最高優先度を与えてDebugCameraを選択させる
		 debugCamera_->SetPriority(kActiveDebugCameraPriority);
	  } else {
		 // 優先度を戻してゲーム用VirtualCameraに戻す
		 debugCamera_->SetPriority(kInactiveDebugCameraPriority);
	  }
   }

   {
	  CinemachineBrain* brain = EngineContext::GetActiveBrain();
	  if (brain && !EngineContext::ShouldRunRuntimeUpdate()) {
		 DebugCamera* editorDrivenCamera = isDebugCameraActive_ ? debugCamera_.get() : nullptr;
		 brain->UpdateEditorPreview(EngineContext::GetUnscaledDeltaTime(), editorDrivenCamera);
	  }
   }
#endif

   OnEditorUpdate();
}

void BaseScene::RuntimeUpdate() {
   const float deltaTime = EngineContext::GetDeltaTime();
   OnUpdate(deltaTime);

   // ターゲットやゲーム状態の更新後にカメラを評価するLate Update相当のフェーズ。
   if (CinemachineBrain* brain = EngineContext::GetActiveBrain()) {
	  brain->Update(deltaTime);
   }
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

   sceneEntities_.clear();
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
   root["version"] = kDebugCameraStateFormatVersion;
   root["sceneName"] = editorSceneName_;
   root["debugCamera"] = debugCamera_->Serialize();
   file << root.dump(kDebugCameraJsonIndentSize);
}
#endif

void BaseScene::UpdateDebugCamera() {
#ifdef USE_IMGUI
   if (EngineContext::IsKeyTriggered(KeyCode::F1)) {
	  isDebugCameraActive_ = !isDebugCameraActive_;
	  if (isDebugCameraActive_) {
		 debugCamera_->SetPriority(kActiveDebugCameraPriority);
	  } else {
		 debugCamera_->SetPriority(kInactiveDebugCameraPriority);
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
