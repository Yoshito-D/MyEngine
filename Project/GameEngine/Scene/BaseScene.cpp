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
#include <Component/Model/AnimationComponent.h>
#include <Component/Particle/ParticleEmitterComponent.h>
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
   // 保存キーは基底Objectの名前だけでは型違いを区別できないため、
   // 派生型を具体的なものから順に判定して安定した型名を付ける。
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
   // 名前未設定でもキーを空にせず、後段の重複サフィックス処理へ渡せる基底キーを作る。
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
   // Edit停止中は通常のComponent更新が走らないが、ライトのGPUプロキシだけは
   // Inspector編集を即座にViewportへ反映する必要があるため個別に同期する。
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

void DrawEditorComponentDebug() {
   const auto registeredObjects = GameEngine::Object::GetRegisteredObjects();
   for (GameEngine::Object* object : registeredObjects) {
	  if (!object) {
		 continue;
	  }

	  if (auto* animation = object->GetComponent<GameEngine::AnimationComponent>();
		 animation && animation->IsEnabled()) {
		 animation->DrawDebugBones();
	  }
	  if (auto* particleEmitter = object->GetComponent<GameEngine::ParticleEmitterComponent>();
		 particleEmitter && particleEmitter->IsEnabled()) {
		 particleEmitter->DrawDebugAttachments();
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

      // Storeが所有する前回ロード分だけを破棄し、シーン固有コードが所有するEntityは残す。
      // 以降、objectsは新規生成、sceneObjectsは既存Entityへの差分として適用される。
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
      // C++側で生成されたEntityをJSON差分と対応付ける。ポインター自体は実行ごとに変わるため、
      // 明示Entity IDを優先し、旧runtime_entity_*だけ型名+表示名から安定キーへ移行する。
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
         // 同名Entityも個別に保存できるよう、登録順に決定的なサフィックスを付ける。
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

      // Registryから外れたParticleSystemの生ポインターを次回ロードへ持ち越さない。
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

      // sceneObjectKeys_は非所有ポインターを保持するため、利用直前に現行Registry所属も確認する。
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
            // C++側所有物はここでdeleteできないため、全Componentを停止し描画も明示的に隠す。
            // 所有者はそのまま保ちつつ、保存上の「削除」と同じ実行結果にする。
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

         // 旧形式は差分本体がobject内、現行形式はentry直下にあるため両方を受け付ける。
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
            // シーン固有コードが所有するため破棄せず、Emissionを停止して削除状態を再現する。
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

      // RuntimeではC++が生成・登録済みのカメラへ状態だけを重ねる。
      // JSONから新しいVirtualCameraを所有すると、シーン終了時の寿命管理が分岐してしまう。
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

         // 名前は並び替えに強いため優先し、名前が一致しない旧データだけindexへフォールバックする。
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

         // 壊れたJSONで同じカメラを複数回上書きし、結果が配列順依存になることを防ぐ。
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

      // 旧環境ライトを現行Entity+LightComponentへ変換する。同IDの既定ライトがあれば再利用し、
      // なければStore所有の汎用Entityを受け皿として作る。
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

   // 各種ライトを必ず同じEntity構造で用意し、Editor保存・親子Transform・Runtime復元の
   // すべてが特殊ケースなしでLightComponentを扱えるようにする。
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
   // CameraUnitが出力CameraとVirtualCamera選択用Brainの寿命を一括管理する。
   CameraUnit* unit = EngineContext::CreateCameraUnit();

   auto mainCamera = std::make_unique<Camera>();
   mainCamera->Initialize();
   mainCamera->SetFarClip(kMainCameraFarClip);
   unit->brain->Initialize(std::move(mainCamera));

#ifdef USE_IMGUI
   // DebugCameraは通常候補より低い優先度で常時登録し、F1時だけ優先度を上げて選択させる。
   // 登録し直さないため、切り替え時にもBrainの候補リストとBlend状態が安定する。
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
   // Editor固有処理を先に反映し、そのフレームのPlayMode状態を使ってRuntimeを進める。
   EditorUpdate();
   RuntimeUpdate();
}

void BaseScene::EditorUpdate() {
#ifdef USE_IMGUI
   if (!EngineContext::ShouldRunRuntimeUpdate()) {
	  // 編集停止中は描画プロキシだけを同期し、ゲームプレイ用Componentを誤って進めない。
	  UpdateEditorLightProxies(EngineContext::GetUnscaledDeltaTime());
	  DrawEditorComponentDebug();
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

   // 派生シーンのEditor処理はPlay/Pause中も呼ぶ。ツールUIの操作をゲーム更新可否と分離する。
   OnEditorUpdate();
}

void BaseScene::RuntimeUpdate() {
   // GetDeltaTimeはPlayModeControllerを通したゲーム時間で、Pause中は0、Step時は1フレーム分になる。
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
   // 派生クラスが自分の登録物を参照できるうちに先に終了処理を通知する。
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

   // Runtime JSONから生成したObjectはStoreが所有するため、Registryの一括クリアより先に破棄する。
   if (runtimeSceneObjectStore_) {
	  runtimeSceneObjectStore_->Clear();
	  runtimeSceneObjectStore_.reset();
   }

   // 既定ライトEntityを破棄してから各Manager/Registryを空にし、生ポインターを残さない。
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
   // Editorビルドは編集コンテキストが差分や履歴を管理し、Releaseビルドは
   // 同じJSONを軽量なRuntimeSceneApplierで直接適用する。
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

   // Runtimeでも既存の復元ロジックを再利用するためStoreを所有コンテナとして使う。
   // 適用失敗時は部分生成されたObjectごと破棄し、C++生成シーンだけで継続する。
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

   // version導入前はCamera本体がルートだったため、ラップ済み形式と旧形式を両方読む。
   const nlohmann::json* cameraData = nullptr;
   if (root.is_object() && root.contains("debugCamera") && root.at("debugCamera").is_object()) {
	  cameraData = &root.at("debugCamera");
   } else if (root.is_object()) {
	  cameraData = &root;
   }
   if (!cameraData) {
	  return;
   }

   // 保存されたpriority/activeを復元するとF1の選択状態まで前回値に引きずられる。
   // Transform等だけを読み、選択制御用の実行時プロパティは初期化時の値を保つ。
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

   // シーンごとに別ファイルへ保存し、作業中の視点を他シーンへ持ち込まない。
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
