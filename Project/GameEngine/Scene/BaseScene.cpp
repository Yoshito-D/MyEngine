#include "GameEngine/pch.h"
#include <cstdint>
#include "GameEngine/Scene/BaseScene.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Scene/SceneObjectStore.h"
#include "GameEngine/Object/Model/Model.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Effects/Particles/ParticleSystem.h"
#include "GameEngine/Object/Sprite/Sprite.h"
#include "GameEngine/Object/Text/UIText.h"
#include "GameEngine/Object/Component/Rendering/RenderComponent.h"
#include "GameEngine/Object/Component/Animation/AnimationComponent.h"
#include "GameEngine/Object/Component/Effects/ParticleEmitterComponent.h"
#include "GameEngine/Object/Component/Rendering/LightComponent.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Skybox/Skybox.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

namespace {
constexpr float kMainCameraFarClip = 10000.0f;
#ifdef USE_IMGUI
constexpr std::int32_t kInactiveDebugCameraPriority = -1;
constexpr std::int32_t kActiveDebugCameraPriority = 100;
constexpr std::int32_t kDebugCameraStateFormatVersion = 1;
constexpr std::int32_t kDebugCameraJsonIndentSize = 3;
#endif

void UpdateEditorLightProxies(float deltaTime) {
   // Edit停止中は通常のComponent更新が走らないが、ライトのGPUプロキシだけは
   // Inspector編集を即座にViewportへ反映する必要があるため個別に同期する。
   const auto registeredObjects = GameEngine::Object::GetRegisteredObjects();

   for (GameEngine::Object* object : registeredObjects) {
      if (!object) {
         continue;
      }

      if (auto* light = object->GetComponent<GameEngine::LightComponent>(); light && light->IsEnabled()) {
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

} // namespace

namespace GameEngine {

BaseScene* BaseScene::sCurrentScene_ = nullptr;

bool BaseScene::sIsWaitingForFadeOut_ = false;

std::string BaseScene::sPendingSceneName_;

std::string BaseScene::sNextSceneName_;
BaseScene::~BaseScene() = default;

void BaseScene::Initialize() {
   // 現在のシーンインスタンスを登録
   sCurrentScene_ = this;

   // CameraUnitを生成（Brain+Cameraのペア）
   // CameraUnitが出力CameraとVirtualCamera選択用Brainの寿命を一括管理する。

   auto mainCamera = std::make_unique<Camera>();
   mainCamera->Initialize();
   mainCamera->SetFarClip(kMainCameraFarClip);
   auto* brain = EngineContext::CreateCameraUnit(std::move(mainCamera));

#ifdef USE_IMGUI
   // DebugCameraは通常候補より低い優先度で常時登録し、F1時だけ優先度を上げて選択させる。
   // 登録し直さないため、切り替え時にもBrainの候補リストとBlend状態が安定する。
   debugCamera_ = std::make_unique<DebugCamera>();
   debugCamera_->Initialize();
   debugCamera_->SetPriority(kInactiveDebugCameraPriority); // 通常時は選ばれない
   LoadDebugCameraState();
   brain->RegisterVirtualCamera(debugCamera_.get());
   brain->SetDefaultBlendTime(0.0f);

   cameraEditor_ = std::make_unique<CameraEditor>();
   cameraEditor_->Initialize(EngineContext::GetLineRenderer());
   cameraEditor_->SetTargetBrain(brain);

   editorSceneContext_ = std::make_unique<EditorSceneContext>(sceneWorld_);
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
   sceneWorld_.Clear();

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

#ifdef USE_IMGUI

std::filesystem::path BaseScene::GetDebugCameraStateFilePath() const {
   const std::string filename = editorSceneName_ + ".json";
   return std::filesystem::path("resources") / "game" / "editor" / "debug_cameras" /
          std::filesystem::path(std::u8string(filename.begin(), filename.end()));
}

void BaseScene::LoadDebugCameraState() {
   if (!debugCamera_) return;

   const std::filesystem::path filePath = GetDebugCameraStateFilePath();

   if (!std::filesystem::exists(filePath)) return;

   std::ifstream file(filePath);

   if (!file.is_open()) return;

   nlohmann::json root;
   try {
      file >> root;
   } catch (...) {
      return;
   }

   if (!root.is_object() || root.value("version", 0) != kDebugCameraStateFormatVersion ||
       !root.contains("debugCamera") || !root.at("debugCamera").is_object())
      return;

   // 保存されたpriority/activeを復元するとF1の選択状態まで前回値に引きずられる。
   // Transform等だけを読み、選択制御用の実行時プロパティは初期化時の値を保つ。
   const std::int32_t priority = debugCamera_->GetPriority();
   const bool active = debugCamera_->IsActive();
   debugCamera_->Deserialize(root.at("debugCamera"));
   debugCamera_->SetName("DebugCamera");
   debugCamera_->SetPriority(priority);
   debugCamera_->SetActive(active);
}

void BaseScene::SaveDebugCameraState() const {
   if (!debugCamera_) return;

   const std::filesystem::path filePath = GetDebugCameraStateFilePath();
   std::error_code error;
   std::filesystem::create_directories(filePath.parent_path(), error);

   if (error) return;

   std::ofstream file(filePath);

   if (!file.is_open()) return;

   // シーンごとに別ファイルへ保存し、作業中の視点を他シーンへ持ち込まない。
   nlohmann::json root = nlohmann::json::object();
   root["version"] = kDebugCameraStateFormatVersion;
   root["sceneName"] = editorSceneName_;
   root["debugCamera"] = debugCamera_->Serialize();
   file << root.dump(kDebugCameraJsonIndentSize);
}
#endif

}
