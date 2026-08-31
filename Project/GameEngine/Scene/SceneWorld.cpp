#include "pch.h"
#include "SceneWorld.h"

#include "Component/TransformComponent.h"
#include "Component/LightComponent.h"
#include "Effect/ParticleSystem.h"
#include "Framework/EngineContext.h"
#include "Model/Model.h"
#include "Object/Object.h"
#include "Object/Skybox/Skybox.h"
#include "Object/Sprite/Sprite.h"
#include "Object/Text/UIText.h"
#include "Scene/Camera/Core/CinemachineBrain.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "Utility/Logger.h"
#include <algorithm>

namespace GameEngine {

SceneWorld::SceneWorld() = default;

SceneWorld::~SceneWorld() {
   Clear();
}

bool SceneWorld::LoadFromJson(const nlohmann::json& sceneData) {
   // 再読み込み時に旧シーンのカメラ登録やID索引を残さないよう、検証より先に所有物を解放する。
   Clear();
   if (!sceneData.is_object()) {
      Logger::Error("SceneWorld load failed: scene root is not an object");
      return false;
   }

   // コンポーネントのデシリアライズ中にも現在のワールドを参照できるよう、
   // 個々のオブジェクトを復元する前に公開する。
   sCurrent_ = this;
   bool loadedAnyObject = false;
   if (sceneData.contains("objects") && sceneData.at("objects").is_array()) {
      for (const auto& objectData : sceneData.at("objects")) {
         loadedAnyObject = RestoreObjectEntry(objectData) || loadedAnyObject;
      }
   }

   // version 3までのsceneObjectsはC++生成物への差分だった。
   // データ駆動シーンでは埋め込まれた完全スナップショットを通常オブジェクトとして復元する。
   // 現行形式と旧形式を同じワールドへ統合した後、カメラやコンポーネント間参照を解決する。
   // 復元途中では参照先がまだ生成されていない場合があるため、この順序を維持する。
   RestoreLegacyEntries(sceneData);
   RestoreLegacyLights(sceneData);
   RestoreCameras(sceneData);
   // 描画設定もシーンの一部として扱う。不正な設定を黙って既定値にすると
   // 見た目だけ異なるシーンが成立してしまうため、ワールド全体の読み込み失敗とする。
   if (sceneData.contains("renderSettings") &&
      !EngineContext::ApplyPostProcessSceneState(sceneData.at("renderSettings"))) {
      Logger::Error("SceneWorld load failed: invalid render settings");
      return false;
   }
   ResolveReferences();

   if (!loadedAnyObject && looseObjectsById_.empty() && objectStore_.SerializeAll().empty()) {
      Logger::EngineWarning("SceneWorld loaded a scene without objects");
   }
   return true;
}

void SceneWorld::Clear() {
   // VirtualCamera はワールドが所有する一方、Brain は非所有ポインターを保持している。
   // unique_ptr を破棄する前に登録解除し、次フレームのダングリング参照を防ぐ。
   if (auto* brain = EngineContext::GetActiveBrain()) {
      for (const auto& camera : virtualCameras_) {
         if (camera) {
            brain->UnregisterVirtualCamera(camera.get());
         }
      }
      // レースのカウントダウン中にシーンが破棄されても、停止状態を次シーンへ持ち越さない。
      brain->SetCameraMotionPaused(false);
   }

   // 逆引きマップには所有コンテナ内の生ポインターが入るため、所有物より先に無効化する。
   virtualCamerasById_.clear();
   virtualCameras_.clear();
   looseObjectIds_.clear();
   looseObjectsById_.clear();
   skyboxes_.clear();
   genericObjects_.clear();
   // EditorObjectStore は削除を遅延できる。Clear後に必ずフラッシュし、
   // 旧シーンのEntityがグローバル検索へ残らない状態まで確定させる。
   objectStore_.Clear();
   objectStore_.FlushDeferredDeletes();
   if (sCurrent_ == this) {
      sCurrent_ = nullptr;
   }
}

void SceneWorld::Update(float) {
   // ObjectコンポーネントはFrameworkの共通Entity更新経路で一度だけ更新する。
}

Object* SceneWorld::FindObjectById(const std::string& objectId) const {
   if (objectId.empty()) {
      return nullptr;
   }
   // Skyboxや旧形式の汎用ObjectはEditorObjectStore外で所有するため、両方の索引を調べる。
   if (auto it = looseObjectsById_.find(objectId); it != looseObjectsById_.end()) {
      return it->second;
   }
   return objectStore_.FindById(objectId);
}

ParticleSystem* SceneWorld::FindParticleSystemById(const std::string& objectId) const {
   if (objectId.empty()) {
      return nullptr;
   }
   return objectStore_.FindParticleById(objectId);
}

Object* SceneWorld::FindObjectByName(const std::string& objectName) const {
   if (objectName.empty()) {
      return nullptr;
   }
   const auto objects = CollectObjects();
   const auto it = std::find_if(objects.begin(), objects.end(),
      [&objectName](const Object* object) {
         return object && object->GetObjectName() == objectName;
      });
   return it != objects.end() ? *it : nullptr;
}

VirtualCamera* SceneWorld::FindVirtualCamera(const std::string& cameraIdOrName) const {
   if (cameraIdOrName.empty()) {
      return nullptr;
   }
   if (auto it = virtualCamerasById_.find(cameraIdOrName); it != virtualCamerasById_.end()) {
      return it->second;
   }
   for (const auto& camera : virtualCameras_) {
      if (camera && camera->GetName() == cameraIdOrName) {
         return camera.get();
      }
   }
   return nullptr;
}

std::string SceneWorld::GetObjectId(const Object* object) const {
   if (!object) {
      return {};
   }
   if (auto it = looseObjectIds_.find(object); it != looseObjectIds_.end()) {
      return it->second;
   }
   return objectStore_.GetId(object);
}

bool SceneWorld::RestoreObjectEntry(const nlohmann::json& sourceData, const std::string& legacySceneKey) {
   if (!sourceData.is_object()) {
      return false;
   }

   // 旧形式の補正値を原文JSONへ書き戻さないよう、エントリー単位の作業コピーを使う。
   nlohmann::json objectData = sourceData;
   std::string objectType = objectData.value("objectType", "Model");
   std::string id = objectData.value("id", legacySceneKey);
   if (id.empty()) {
      Logger::EngineWarning("Scene object skipped because it has no stable id");
      return false;
   }
   // シーン初期化コードが先に生成したEntityと同じIDなら、新規生成せず保存状態だけを重ねる。
   // FindObjectByIdで当ワールド未登録であることも確認し、重複エントリーとの区別を保つ。
   if (Object* existingEntity = Object::FindByEntityId(id);
      existingEntity && !FindObjectById(id)) {
      return objectStore_.ApplyObjectState(existingEntity, objectData);
   }
   if (FindObjectById(id) || objectStore_.FindParticleById(id)) {
      Logger::EngineWarning("Duplicate scene object id skipped: " + id);
      return false;
   }

   // 古いファイルにはSkybox専用typeがなく、sceneKeyの接頭辞だけが識別情報だった。
   if ((objectType == "Object" || objectType == "Generic") && legacySceneKey.rfind("Skybox:", 0) == 0) {
      objectType = "Skybox";
      objectData["objectType"] = objectType;
   }

   // 汎用ObjectとSkyboxはEditorObjectStoreの型別コンテナに入らないため、
   // SceneWorld自身が所有し、loose object用の双方向索引へ登録する。
   if (objectType == "Generic" || objectType == "Object") {
      auto object = std::make_unique<Object>();
      Object* rawObject = object.get();
      rawObject->SetEntityId(id);
      rawObject->SetObjectName(id);
      if (objectData.contains("components") && objectData.at("components").is_array()) {
         rawObject->DeserializeComponents(objectData.at("components"));
      }
      if (objectData.contains("parentId") && objectData.at("parentId").is_string()) {
         rawObject->SetParentEntityId(objectData.at("parentId").get<std::string>());
      }
      RegisterLooseObject(id, rawObject);
      genericObjects_.push_back(std::move(object));
      return true;
   }

   if (objectType == "Skybox") {
      auto skybox = std::make_unique<Skybox>();
      Skybox* rawSkybox = skybox.get();
      rawSkybox->SetEntityId(id);
      rawSkybox->Create(EngineContext::GetGraphicsDevice());
      rawSkybox->SetObjectName(id);
      if (objectData.contains("components") && objectData.at("components").is_array()) {
         rawSkybox->DeserializeComponents(objectData.at("components"));
      }
      if (objectData.contains("parentId") && objectData.at("parentId").is_string()) {
         rawSkybox->SetParentEntityId(objectData.at("parentId").get<std::string>());
      }
      RegisterLooseObject(id, rawSkybox);
      skyboxes_.push_back(std::move(skybox));
      return true;
   }

   // ParticleSystemは通常Objectとは別の所有コンテナを使うので専用復元経路へ振り分ける。
   if (objectType == "ParticleSystem") {
      return objectStore_.RestoreParticleSystem(objectData) != nullptr;
   }

   return objectStore_.RestoreObject(objectData) != nullptr;
}

void SceneWorld::RestoreLegacyEntries(const nlohmann::json& sceneData) {
   // 旧sceneObjectsは削除差分も含む。完全スナップショットへ移行する際は
   // deleted項目を生成せず、生存エントリーの埋め込み本体だけを復元する。
   if (sceneData.contains("sceneObjects") && sceneData.at("sceneObjects").is_array()) {
      // 現行objectsのSkyboxを優先し、後から読む旧差分が全画面背景を上書きしないようにする。
      const bool hasCurrentFormatSkybox = !skyboxes_.empty();
      for (const auto& entry : sceneData.at("sceneObjects")) {
         if (!entry.is_object() || entry.value("deleted", false) ||
            !entry.contains("object") || !entry.at("object").is_object()) {
            continue;
         }

         const auto& objectData = entry.at("object");
         const std::string sceneKey = entry.value("sceneKey", "");
         const bool isSkybox = objectData.value("objectType", "") == "Skybox" ||
            sceneKey.rfind("Skybox:", 0) == 0;
         if (hasCurrentFormatSkybox && isSkybox) {
            continue;
         }

         RestoreObjectEntry(objectData, sceneKey);
      }
   }

   if (sceneData.contains("sceneParticleSystems") && sceneData.at("sceneParticleSystems").is_array()) {
      for (const auto& entry : sceneData.at("sceneParticleSystems")) {
         if (!entry.is_object() || entry.value("deleted", false) ||
            !entry.contains("particleSystem") || !entry.at("particleSystem").is_object()) {
            continue;
         }
         RestoreObjectEntry(entry.at("particleSystem"), entry.value("sceneKey", ""));
      }
   }
}

void SceneWorld::RestoreLegacyLights(const nlohmann::json& sceneData) {
   if (!sceneData.contains("environment") || !sceneData.at("environment").is_object()) {
      return;
   }
   const auto& environment = sceneData.at("environment");
   if (!environment.contains("lights") || !environment.at("lights").is_array()) {
      return;
   }

   // 旧ライトは環境設定直下にありEntityを持たなかった。現行Component形式へ移すため、
   // 同IDのEntityを再利用するか、Transform付きの受け皿Entityを生成する。
   for (const auto& lightData : environment.at("lights")) {
      if (!lightData.is_object()) {
         continue;
      }
      const std::string id = lightData.value("id", "");
      if (id.empty()) {
         continue;
      }

      Object* entity = Object::FindByEntityId(id);
      if (!entity) {
         auto newEntity = std::make_unique<Object>();
         entity = newEntity.get();
         entity->SetEntityId(id);
         entity->SetObjectName(id);
         entity->AddComponent<TransformComponent>();
         RegisterLooseObject(id, entity);
         genericObjects_.push_back(std::move(newEntity));
      }

      auto* light = entity->GetComponent<LightComponent>();
      if (!light) {
         light = entity->AddComponent<LightComponent>();
      }
      if (light) {
         light->DeserializeLegacy(lightData);
      }
   }
}

void SceneWorld::RestoreCameras(const nlohmann::json& sceneData) {
   if (!sceneData.contains("cameras") || !sceneData.at("cameras").is_object()) {
      return;
   }

   // VirtualCameraはBrainへ登録されて初めて評価対象になる。Brain不在時に
   // 半端なカメラだけ所有しても機能しないため、まとめて読み飛ばす。
   auto* brain = EngineContext::GetActiveBrain();
   if (!brain) {
      Logger::EngineWarning("Virtual cameras skipped because no active camera brain exists");
      return;
   }

   const auto& camerasData = sceneData.at("cameras");
   if (camerasData.contains("brain") && camerasData.at("brain").is_object()) {
      const auto& brainData = camerasData.at("brain");
      if (brainData.contains("defaultBlendTime") && brainData.at("defaultBlendTime").is_number()) {
         brain->SetDefaultBlendTime(brainData.at("defaultBlendTime").get<float>());
      }
   }

   if (!camerasData.contains("virtualCameras") || !camerasData.at("virtualCameras").is_array()) {
      return;
   }

   for (const auto& cameraData : camerasData.at("virtualCameras")) {
      // DebugCameraはエディター側が別途所有・復元するため、シーンデータから二重生成しない。
      if (!cameraData.is_object() || cameraData.value("name", "") == "DebugCamera") {
         continue;
      }
      auto camera = std::make_unique<VirtualCamera>();
      VirtualCamera* rawCamera = camera.get();
      rawCamera->Initialize();
      rawCamera->Deserialize(cameraData);
      const std::string id = cameraData.value("id", rawCamera->GetName());
      if (id.empty() || virtualCamerasById_.contains(id)) {
         Logger::EngineWarning("Virtual camera skipped because its id is empty or duplicated");
         continue;
      }

      // BrainとID索引は非所有ポインターを保持する。最後にunique_ptrをコンテナへ移し、
      // 以降の参照先アドレスがワールド破棄まで安定するようにする。
      brain->RegisterVirtualCamera(rawCamera);
      virtualCamerasById_[id] = rawCamera;
      virtualCameras_.push_back(std::move(camera));
   }
}

void SceneWorld::ResolveReferences() {
   // 全Entityの生成後に参照を結ぶ二段階ロード。デシリアライズ順に依存せず、
   // 子が親より先に記録されたJSONでも同じ結果になる。
   for (Object* object : CollectObjects()) {
      if (!object) {
         continue;
      }
      // parentObjectNameは安定ID導入前の互換フィールド。解決できた時点でIDへ移し、
      // 以後の保存で名前変更の影響を受けないよう旧参照を消す。
      if (object->GetParentEntityId().empty()) {
         if (auto* transform = object->GetComponent<TransformComponent>();
            transform && !transform->parentObjectName.empty()) {
            if (Object* parent = Object::FindByObjectName(transform->parentObjectName)) {
               object->SetParentEntityId(parent->GetEntityId());
               transform->parentObjectName.clear();
            }
         }
      }
      // 親子関係など基礎参照を先に確定し、その後で各コンポーネント固有の参照を通知する。
      for (const auto& component : object->GetComponentContainer().GetAll()) {
         if (component) {
            component->OnSceneLoaded(*this);
         }
      }
   }
}

std::vector<Object*> SceneWorld::CollectObjects() const {
   // 所有場所の違いを利用側へ漏らさないため、一時的な統合ビューを構築する。
   // ParticleSystemはObject継承ではないので、この一覧には意図的に含めない。
   std::vector<Object*> objects;
   objects.reserve(
      objectStore_.GetModels().size() + objectStore_.GetSprites().size() +
      objectStore_.GetUITexts().size() + objectStore_.GetGenericObjects().size() +
      genericObjects_.size() + skyboxes_.size());
   for (const auto& model : objectStore_.GetModels()) {
      if (model) { objects.push_back(model.get()); }
   }
   for (const auto& sprite : objectStore_.GetSprites()) {
      if (sprite) { objects.push_back(sprite.get()); }
   }
   for (const auto& text : objectStore_.GetUITexts()) {
      if (text) { objects.push_back(text.get()); }
   }
   for (const auto& object : objectStore_.GetGenericObjects()) {
      if (object) { objects.push_back(object.get()); }
   }
   for (const auto& object : genericObjects_) {
      if (object) { objects.push_back(object.get()); }
   }
   for (const auto& skybox : skyboxes_) {
      if (skybox) { objects.push_back(skybox.get()); }
   }
   return objects;
}

void SceneWorld::RegisterLooseObject(const std::string& id, Object* object) {
   if (id.empty() || !object) {
      return;
   }
   // ID→Objectは参照解決、Object→IDは保存処理で使用するため、必ず同時に更新する。
   looseObjectsById_[id] = object;
   looseObjectIds_[object] = id;
   object->SetEntityId(id);
}

} // namespace GameEngine
