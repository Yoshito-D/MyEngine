#include "GameEngine/pch.h"
#include <cstdint>
#include "GameEngine/Scene/SceneObjectStore.h"
#include "GameEngine/Scene/SceneWorld.h"

#include "GameEngine/Object/Component/Rendering/MaterialComponent.h"
#include "GameEngine/Object/Component/Effects/ParticleEmitterComponent.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Effects/Particles/ParticleSystem.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Object/Model/Model.h"
#include "GameEngine/Object/Skybox/Skybox.h"
#include "GameEngine/Object/Sprite/Sprite.h"
#include "GameEngine/Object/Text/UIText.h"
#include <algorithm>
#include <filesystem>

namespace GameEngine {

namespace {
std::string BuildNameFromAssetId(const std::string& assetId) {
   if (assetId.empty()) {
      return "EditorModel";
   }

   const auto name = std::filesystem::path(std::u8string(assetId.begin(), assetId.end())).stem().u8string();
   return std::string(reinterpret_cast<const char*>(name.data()), name.size());
}

} // namespace

SceneObjectStore::SceneObjectStore(SceneWorld& world) : world_(world) {
   // ファクトリは呼び出し元ストアを受け取り、別Worldの所有先や寿命をキャプチャーしない。
   RegisterObjectFactory("Generic", [](SceneObjectStore& store, const nlohmann::json& data) {
      return store.CreateGenericObject(nullptr, data.value("id", ""));
   });
   RegisterObjectFactory("Model", [](SceneObjectStore& store, const nlohmann::json& data) {
      const std::string assetId = EngineContext::GetParameterAssetStore().ResolveObjectAssetId(data);
      return store.CreateModel(assetId, nullptr, data.value("id", ""));
   });
   RegisterObjectFactory("Sprite", [](SceneObjectStore& store, const nlohmann::json& data) {
      const std::string assetId = EngineContext::GetParameterAssetStore().ResolveObjectAssetId(data);
      return store.CreateSprite(assetId, nullptr, data.value("id", ""));
   });
   RegisterObjectFactory("UIText", [](SceneObjectStore& store, const nlohmann::json& data) {
      return store.CreateUIText(nullptr, data.value("id", ""));
   });
   RegisterObjectFactory("Skybox",
      [](SceneObjectStore& store, const nlohmann::json& data) { return store.CreateSkybox(data.value("id", "")); });
}

SceneObjectStore::~SceneObjectStore() = default;

bool SceneObjectStore::RegisterObjectFactory(const std::string& objectType, ObjectFactory factory) {
   if (objectType.empty() || !factory) {
      return false;
   }

   objectFactories_.insert_or_assign(objectType, std::move(factory));
   return true;
}

Object* SceneObjectStore::AdoptObject(std::unique_ptr<Object> object, const std::string& requestedId) {
   if (!object) {
      return nullptr;
   }

   Object* rawObject = object.get();
   const std::string id = AllocateId(requestedId);
   RegisterObject(id, rawObject);
   objects_.push_back(std::move(object));
   return rawObject;
}

Object* SceneObjectStore::CreateGenericObject(const Transform* initialTransform, const std::string& requestedId) {
   auto object = std::make_unique<Object>();
   Object* rawObject = object.get();
   auto* transformComponent = rawObject->AddComponent<TransformComponent>();

   if (initialTransform && transformComponent) {
      transformComponent->ApplyLocalPose(*initialTransform);
   }

   rawObject->SetObjectName(BuildUniqueObjectName("EmptyObject"));

   return AdoptObject(std::move(object), requestedId);
}

Object* SceneObjectStore::CreateModel(
   const std::string& assetId, const Transform* initialTransform, const std::string& requestedId) {
   std::shared_ptr<ModelAsset> modelAsset;

   // 読み込み失敗時に空のModelを各レジストリへ残さないよう、所有オブジェクト生成より先にアセットを解決する。
   if (!assetId.empty()) {
      modelAsset = EngineContext::LoadModelByAssetId(assetId);

      if (!modelAsset) {
         return nullptr;
      }
   }

   auto model = std::make_unique<Model>();
   Model* rawModel = model.get();
   rawModel->Create();

   if (modelAsset) {
      rawModel->SetModelAsset(modelAsset);
   }

   rawModel->SetObjectName(BuildUniqueObjectName(assetId.empty() ? "Model" : BuildNameFromAssetId(assetId)));

   if (initialTransform) {
      rawModel->SetTransform(*initialTransform);
   }

   return AdoptObject(std::move(model), requestedId);
}

Object* SceneObjectStore::CreateSprite(
   const std::string& textureAssetId, const Transform* initialTransform, const std::string& requestedId) {
   if (textureAssetId.empty()) {
      return nullptr;
   }

   Texture* texture = EngineContext::GetTexture(textureAssetId);

   if (!texture || texture->GetMetadata().IsCubemap()) {
      return nullptr;
   }

   const Vector2 spriteSize(static_cast<float>(texture->GetWidth()), static_cast<float>(texture->GetHeight()));

   auto sprite = std::make_unique<Sprite>();
   Sprite* rawSprite = sprite.get();
   rawSprite->Create(spriteSize, nullptr, Vector2(0.5f, 0.5f));
   rawSprite->SetObjectName(BuildUniqueObjectName(BuildNameFromAssetId(textureAssetId)));

   if (auto* materialComponent = rawSprite->GetComponent<MaterialComponent>()) {
      materialComponent->SetTextureName(textureAssetId);
   }

   if (initialTransform) {
      if (auto* transformComponent = rawSprite->GetComponent<TransformComponent>()) {
         transformComponent->ApplyLocalPose(*initialTransform);
      }
   }

   return AdoptObject(std::move(sprite), requestedId);
}

Object* SceneObjectStore::CreateUIText(const Transform* initialTransform, const std::string& requestedId) {
   auto uiText = std::make_unique<UIText>();
   UIText* rawText = uiText.get();

   TextStyle style{};

   // 空のFont IDで生成に失敗しないよう、現在登録済みの先頭フォントを初期値にする。
   const auto fontIds = EngineContext::GetFontIds();

   if (!fontIds.empty()) {
      style.fontId = fontIds.front();
   }

   rawText->Create("Text", style);
   rawText->SetObjectName(BuildUniqueObjectName("UIText"));

   if (initialTransform) {
      if (auto* transformComponent = rawText->GetComponent<TransformComponent>()) {
         transformComponent->ApplyLocalPose(*initialTransform);
      }
   }

   return AdoptObject(std::move(uiText), requestedId);
}

Object* SceneObjectStore::CreateSkybox(const std::string& requestedId) {
   GraphicsDevice* graphicsDevice = EngineContext::GetGraphicsDevice();

   if (!graphicsDevice) {
      return nullptr;
   }

   auto skybox = std::make_unique<Skybox>();
   Skybox* rawSkybox = skybox.get();
   rawSkybox->Create(graphicsDevice);

   return AdoptObject(std::move(skybox), requestedId);
}

ParticleSystem* SceneObjectStore::CreateParticleSystem(
   const std::string& assetId, const std::string& requestedId, const Transform* initialTransform) {
   auto particleSystem = std::make_unique<ParticleSystem>();
   ParticleSystem* rawParticleSystem = particleSystem.get();
   rawParticleSystem->SetWorld(&world_);
   rawParticleSystem->Create();
   rawParticleSystem->SetName(
      BuildUniqueObjectName(assetId.empty() ? "ParticleSystem" : BuildNameFromAssetId(assetId)));

   if (!assetId.empty()) {
      if (!rawParticleSystem->LoadFromJson((std::filesystem::path("resources") / assetId).generic_string())) {
         return nullptr;
      }
   }

   // アセット内のShape位置よりユーザーがドロップした配置を優先するため、JSON読込後にTransformを上書きする。
   if (initialTransform) {
      rawParticleSystem->ApplyEmitterPose(*initialTransform);
   }

   rawParticleSystem->Play();

   const std::string id = AllocateId(requestedId);
   RegisterParticleSystem(id, rawParticleSystem, assetId);
   particleSystems_.push_back(std::move(particleSystem));
   return rawParticleSystem;
}

Object* SceneObjectStore::RestoreObject(const nlohmann::json& objectData) {
   if (!objectData.is_object() || !objectData.contains("objectType") || !objectData.at("objectType").is_string()) {
      return nullptr;
   }

   const auto& objectType = objectData.at("objectType").get_ref<const std::string&>();
   const auto factory = objectFactories_.find(objectType);

   if (factory == objectFactories_.end()) {
      return nullptr;
   }

   Object* object = nullptr;

   try {
      // 派生型ごとの必須リソースを先に生成してから、共通コンポーネント状態を上書きする。
      object = factory->second(*this, objectData);

      if (object && Contains(object) && ApplyObjectState(object, objectData)) {
         return object;
      }
   } catch (const std::exception& error) {
      Logger::Error("Object restore failed (" + objectType + "): " + error.what());
   }

   if (object && Contains(object)) {
      // 途中まで復元した実体を検索・描画対象へ残さず、GPU参照を保つ遅延削除経路で片付ける。
      DeleteObject(GetId(object));
   }

   return nullptr;
}

ParticleSystem* SceneObjectStore::RestoreParticleSystem(const nlohmann::json& objectData) {
   if (!objectData.is_object()) {
      return nullptr;
   }

   const std::string assetId = objectData.value("assetId", "");
   const std::string id = objectData.value("id", "");
   const bool hasSnapshot = objectData.contains("data") && objectData.at("data").is_object();

   // 完全な保存状態があれば元テンプレートの再読込を省き、ファイルの削除後でもUndo可能にする。
   ParticleSystem* particleSystem = CreateParticleSystem(hasSnapshot ? std::string{} : assetId, id);

   if (particleSystem) {
      particleSystemAssetIds_[particleSystem] = assetId;
   }

   // assetIdはテンプレート生成用、dataは保存時点の完全な編集状態なので後者を最後に適用する。
   if (particleSystem && objectData.contains("data") && objectData.at("data").is_object()) {
      particleSystem->FromJson(objectData.at("data"));
   }

   if (particleSystem && objectData.contains("name") && objectData.at("name").is_string()) {
      particleSystem->SetName(objectData.at("name").get<std::string>());
   }

   return particleSystem;
}

bool SceneObjectStore::DeleteObject(const std::string& objectId) {
   Object* target = FindById(objectId);

   if (!target) {
      return false;
   }
   const auto it =
      std::find_if(objects_.begin(), objects_.end(), [target](const auto& p) { return p.get() == target; });

   if (it == objects_.end()) {
      return false;
   }

   UnregisterObject(target);
   UnregisterOwnedRuntimeSystems(target);

   // 型固有の描画登録は実体が存続している間に解除し、GPU参照の完了後まで破棄を遅らせる。
   target->OnRemovedFromScene();
   deferredDeleteObjects_.push_back(std::move(*it));
   objects_.erase(it);
   return true;
}

bool SceneObjectStore::DeleteParticleSystem(const std::string& objectId) {
   auto mapIt = idToParticleSystem_.find(objectId);

   if (mapIt == idToParticleSystem_.end()) {
      return false;
   }

   ParticleSystem* target = mapIt->second;
   auto vecIt = std::find_if(particleSystems_.begin(), particleSystems_.end(),
      [target](const std::unique_ptr<ParticleSystem>& particleSystem) { return particleSystem.get() == target; });

   if (vecIt == particleSystems_.end()) {
      return false;
   }

   // Store内の検索表とグローバル描画レジストリを先に外し、実体はフレーム境界まで保持する。
   UnregisterParticleSystem(target);
   ParticleSystem::UnregisterParticleSystem(target);
   deferredDeleteParticleSystems_.push_back(std::move(*vecIt));
   particleSystems_.erase(vecIt);
   return true;
}

void SceneObjectStore::FlushDeferredDeletes() {
   deferredDeleteObjects_.clear();
   deferredDeleteParticleSystems_.clear();
   pendingDeletionObjects_.clear();
}

void SceneObjectStore::Clear() {
   // 個別削除とシーン全体の置換で、登録解除と遅延削除の経路を統一する。
   while (!objects_.empty()) {
      DeleteObject(GetId(objects_.back().get()));
   }
   while (!particleSystems_.empty()) {
      DeleteParticleSystem(GetId(particleSystems_.back().get()));
   }
}

bool SceneObjectStore::Contains(const Object* object) const {
   return object && objectToId_.contains(object);
}

bool SceneObjectStore::Contains(const ParticleSystem* particleSystem) const {
   return particleSystem && particleSystemToId_.contains(particleSystem);
}

bool SceneObjectStore::ContainsId(const std::string& objectId) const {
   // 描画用の補助オブジェクトも同じID索引を使用するため、Store外の実体との衝突も拒否する。
   return idToObject_.contains(objectId) || idToParticleSystem_.contains(objectId) ||
          Object::FindByEntityId(objectId) != nullptr;
}

std::string SceneObjectStore::GetId(const Object* object) const {
   auto it = objectToId_.find(object);

   if (it == objectToId_.end()) {
      return {};
   }

   return it->second;
}

std::string SceneObjectStore::GetId(const ParticleSystem* particleSystem) const {
   auto it = particleSystemToId_.find(particleSystem);

   if (it == particleSystemToId_.end()) {
      return {};
   }

   return it->second;
}

Object* SceneObjectStore::FindById(const std::string& objectId) {
   auto it = idToObject_.find(objectId);

   if (it == idToObject_.end()) {
      return nullptr;
   }

   return it->second;
}

const Object* SceneObjectStore::FindById(const std::string& objectId) const {
   auto it = idToObject_.find(objectId);

   if (it == idToObject_.end()) {
      return nullptr;
   }

   return it->second;
}

ParticleSystem* SceneObjectStore::FindParticleById(const std::string& objectId) {
   auto it = idToParticleSystem_.find(objectId);

   if (it == idToParticleSystem_.end()) {
      return nullptr;
   }

   return it->second;
}

const ParticleSystem* SceneObjectStore::FindParticleById(const std::string& objectId) const {
   auto it = idToParticleSystem_.find(objectId);

   if (it == idToParticleSystem_.end()) {
      return nullptr;
   }

   return it->second;
}

nlohmann::json SceneObjectStore::SerializeObject(const std::string& objectId) const {
   auto objectIt = idToObject_.find(objectId);

   if (objectIt == idToObject_.end()) {
      auto particleIt = idToParticleSystem_.find(objectId);

      if (particleIt == idToParticleSystem_.end() || !particleIt->second) {
         return nlohmann::json::object();
      }

      std::string assetId;

      if (auto assetIt = particleSystemAssetIds_.find(particleIt->second); assetIt != particleSystemAssetIds_.end()) {
         assetId = assetIt->second;
      }

      return SerializeParticleSystemState(particleIt->second, objectId, assetId);
   }

   if (!objectIt->second) {
      return nlohmann::json::object();
   }

   return SerializeObjectState(objectIt->second, objectId);
}

nlohmann::json SceneObjectStore::SerializeObjectState(const Object* object, const std::string& id) const {
   if (!object) {
      return nlohmann::json::object();
   }

   nlohmann::json objectData = object->SerializeObjectData();
   objectData["id"] = id.empty() ? object->GetEntityId() : id;
   objectData["parentId"] = object->GetParentEntityId();

   // 復元前に正しい具象型を生成できるよう、Object共通データとは別に安定した種別名を保存する。
   objectData["objectType"] = ToObjectTypeName(object->GetObjectType());
   objectData["components"] = object->SerializeComponents();
   return objectData;
}

bool SceneObjectStore::ApplyObjectState(Object* object, const nlohmann::json& objectData) const {
   if (!object || !objectData.is_object()) {
      return false;
   }

   // 異なる具象型へスナップショットを適用すると専用データが欠落するため、先に型整合性を検証する。
   if (!objectData.contains("objectType") || !objectData.at("objectType").is_string() ||
       objectData.at("objectType") != ToObjectTypeName(object->GetObjectType())) {
      return false;
   }

   if (objectData.contains("components") && objectData.at("components").is_array()) {
      if (!object->DeserializeComponents(objectData.at("components"))) {
         return false;
      }
   }

   if (objectData.contains("parentId") && objectData.at("parentId").is_string()) {
      object->SetParentEntityId(objectData.at("parentId").get<std::string>());
   }

   object->DeserializeObjectData(objectData);
   return true;
}

nlohmann::json SceneObjectStore::SerializeParticleSystemState(
   const ParticleSystem* particleSystem, const std::string& id, const std::string& assetId) const {
   if (!particleSystem) {
      return nlohmann::json::object();
   }

   return nlohmann::json{{"id", id}, {"objectType", "ParticleSystem"}, {"name", particleSystem->GetName()},
      {"assetId", assetId}, {"data", particleSystem->ToJson()}};
}

bool SceneObjectStore::ApplyParticleSystemState(
   ParticleSystem* particleSystem, const nlohmann::json& objectData) const {
   if (!particleSystem || !objectData.is_object()) {
      return false;
   }

   if (objectData.contains("data") && objectData.at("data").is_object()) {
      particleSystem->FromJson(objectData.at("data"));
   }

   if (objectData.contains("name") && objectData.at("name").is_string()) {
      particleSystem->SetName(objectData.at("name").get<std::string>());
   }

   return true;
}

nlohmann::json SceneObjectStore::SerializeAll() const {
   auto result = nlohmann::json::array();
   for (const auto& object : objects_) {
      result.push_back(SerializeObject(GetId(object.get())));
   }
   for (const auto& particle : particleSystems_) {
      result.push_back(SerializeObject(GetId(particle.get())));
   }
   return result;
}

std::vector<Object*> SceneObjectStore::GetObjects() {
   std::vector<Object*> result;
   for (const auto& entry : objects_) {
      result.push_back(entry.get());
   }
   return result;
}

std::vector<const Object*> SceneObjectStore::GetObjects() const {
   std::vector<const Object*> result;
   for (const auto& entry : objects_) {
      result.push_back(entry.get());
   }
   return result;
}

std::string SceneObjectStore::AllocateId(const std::string& requestedId) {
   if (!requestedId.empty() && !ContainsId(requestedId) && !Object::FindByEntityId(requestedId)) {
      // 復元IDの番号を採用した後に自動採番が衝突しないようカウンターも追従させる。
      BumpCounterFromId(requestedId);
      return requestedId;
   }

   while (true) {
      const std::string id = "editor_object_" + std::to_string(nextObjectIndex_++);

      if (!ContainsId(id) && !Object::FindByEntityId(id)) {
         return id;
      }
   }
}

std::string SceneObjectStore::BuildUniqueObjectName(const std::string& baseName) const {
   const std::string base = baseName.empty() ? "EditorObject" : baseName;

   auto exists = [](const std::string& name) {
      // Entity種別に依存せず、ヒエラルキー全体で表示名を一意にする。
      for (const auto* object : Object::GetRegisteredObjects()) {
         if (object && object->GetObjectName() == name) {
            return true;
         }
      }

      for (const auto* particleSystem : ParticleSystem::GetRegisteredParticleSystems()) {
         if (particleSystem && particleSystem->GetName() == name) {
            return true;
         }
      }

      return false;
   };

   if (!exists(base)) {
      return base;
   }

   std::int32_t suffix = 1;

   while (true) {
      const std::string candidate = base + "_" + std::to_string(suffix++);

      if (!exists(candidate)) {
         return candidate;
      }
   }
}

void SceneObjectStore::RegisterObject(const std::string& id, Object* object) {
   if (!object || id.empty()) return;

   // ID検索とポインター検索の双方向表を同時に更新し、Undoコマンドのどちらの参照経路も一致させる。
   if (!object->SetEntityId(id)) {
      throw std::runtime_error("Duplicate entity ID: " + id);
   }
   object->SetWorld(&world_);
   idToObject_[id] = object;
   objectToId_[object] = id;
}

void SceneObjectStore::RegisterParticleSystem(
   const std::string& id, ParticleSystem* particleSystem, const std::string& assetId) {
   if (!particleSystem || id.empty()) return;

   // 再保存時に元テンプレートも保持できるよう、識別表とassetIdを同じ寿命で登録する。
   idToParticleSystem_[id] = particleSystem;
   particleSystemToId_[particleSystem] = id;
   particleSystemAssetIds_[particleSystem] = assetId;
}

void SceneObjectStore::UnregisterObject(Object* object) {
   if (!object) return;

   auto objectIt = objectToId_.find(object);

   if (objectIt != objectToId_.end()) {
      const std::string oldId = objectIt->second;
      idToObject_.erase(oldId);
      objectToId_.erase(objectIt);

      // 実体は描画フレーム末まで残すが、Undoは同じ安定IDを即座に復元できる必要がある。
      // 子は先にルートへ戻し、一時IDの伝播や旧実体の破棄による再解除を防ぐ。
      for (auto* child : Object::GetRegisteredObjects()) {
         if (child && child != object && child->GetParentEntityId() == oldId) {
            child->SetParentEntityId({});
         }
      }

      std::string retiredId;

      do {
         retiredId = "pending_delete_" + oldId + "_" + std::to_string(nextObjectIndex_++);
      } while (Object::FindByEntityId(retiredId));

      object->SetEntityId(retiredId);
   }
}

void SceneObjectStore::UnregisterParticleSystem(ParticleSystem* particleSystem) {
   if (!particleSystem) return;

   auto objectIt = particleSystemToId_.find(particleSystem);

   if (objectIt != particleSystemToId_.end()) {
      idToParticleSystem_.erase(objectIt->second);
      particleSystemToId_.erase(objectIt);
   }

   particleSystemAssetIds_.erase(particleSystem);
}

void SceneObjectStore::UnregisterOwnedRuntimeSystems(Object* object) {
   if (!object) return;

   pendingDeletionObjects_.insert(object);

   // 親Objectより長く静的描画リストへ残らないよう、Emitterが所有する実行時システムも先に解除する。
   if (auto* emitter = object->GetComponent<ParticleEmitterComponent>()) {
      emitter->UnregisterParticleSystemsForRender();
   }
}

void SceneObjectStore::BumpCounterFromId(const std::string& id) {
   constexpr const char* kPrefix = "editor_object_";
   const std::string prefix = kPrefix;

   if (id.rfind(prefix, 0) != 0) return;

   const std::string numberText = id.substr(prefix.size());

   if (numberText.empty()) return;

   try {
      const uint64_t number = std::stoull(numberText);
      nextObjectIndex_ = std::max(nextObjectIndex_, number + 1);
   } catch (...) {
      // 接頭辞が同じでも数値でない外部IDは有効な識別子として残し、自動採番だけを変更しない。
   }
}

} // namespace GameEngine

namespace GameEngine {

std::vector<Object*> SceneObjectStore::GetGenericObjects() {
   std::vector<Object*> result;
   for (const auto& entry : objects_) {
      if (entry->GetObjectType() == ObjectType::Generic) {
         if (auto* value = dynamic_cast<Object*>(entry.get())) {
            result.push_back(value);
         }
      }
   }
   return result;
}

std::vector<const Object*> SceneObjectStore::GetGenericObjects() const {
   std::vector<const Object*> result;
   for (const auto& entry : objects_) {
      if (entry->GetObjectType() == ObjectType::Generic) {
         if (auto* value = dynamic_cast<const Object*>(entry.get())) {
            result.push_back(value);
         }
      }
   }
   return result;
}

std::vector<Model*> SceneObjectStore::GetModels() {
   std::vector<Model*> result;

   for (const auto& entry : objects_) {
      if (auto* model = dynamic_cast<Model*>(entry.get())) {
         result.push_back(model);
      }
   }

   return result;
}

std::vector<const Model*> SceneObjectStore::GetModels() const {
   std::vector<const Model*> result;

   for (const auto& entry : objects_) {
      if (auto* model = dynamic_cast<const Model*>(entry.get())) {
         result.push_back(model);
      }
   }

   return result;
}

std::vector<Sprite*> SceneObjectStore::GetSprites() {
   std::vector<Sprite*> result;

   for (const auto& entry : objects_) {
      if (auto* sprite = dynamic_cast<Sprite*>(entry.get())) {
         result.push_back(sprite);
      }
   }

   return result;
}

std::vector<const Sprite*> SceneObjectStore::GetSprites() const {
   std::vector<const Sprite*> result;

   for (const auto& entry : objects_) {
      if (auto* sprite = dynamic_cast<const Sprite*>(entry.get())) {
         result.push_back(sprite);
      }
   }

   return result;
}

std::vector<UIText*> SceneObjectStore::GetUITexts() {
   std::vector<UIText*> result;

   for (const auto& entry : objects_) {
      if (auto* text = dynamic_cast<UIText*>(entry.get())) {
         result.push_back(text);
      }
   }

   return result;
}

std::vector<const UIText*> SceneObjectStore::GetUITexts() const {
   std::vector<const UIText*> result;

   for (const auto& entry : objects_) {
      if (auto* text = dynamic_cast<const UIText*>(entry.get())) {
         result.push_back(text);
      }
   }

   return result;
}

std::vector<Skybox*> SceneObjectStore::GetSkyboxes() {
   std::vector<Skybox*> result;

   for (const auto& entry : objects_) {
      if (auto* skybox = dynamic_cast<Skybox*>(entry.get())) {
         result.push_back(skybox);
      }
   }

   return result;
}

std::vector<const Skybox*> SceneObjectStore::GetSkyboxes() const {
   std::vector<const Skybox*> result;

   for (const auto& entry : objects_) {
      if (auto* skybox = dynamic_cast<const Skybox*>(entry.get())) {
         result.push_back(skybox);
      }
   }

   return result;
}

std::vector<ParticleSystem*> SceneObjectStore::GetParticleSystems() {
   std::vector<ParticleSystem*> view;

   for (auto& entry : particleSystems_) {
      view.push_back(entry.get());
   }

   return view;
}

std::vector<const ParticleSystem*> SceneObjectStore::GetParticleSystems() const {
   std::vector<const ParticleSystem*> view;

   for (const auto& entry : particleSystems_) {
      view.push_back(entry.get());
   }

   return view;
}

} // namespace GameEngine
