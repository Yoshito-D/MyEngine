#include "GameEngine/pch.h"
#include "GameEngine/Scene/SceneObjectStore.h"

#include "GameEngine/Object/Component/Rendering/MeshComponent.h"
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

const char* ToSpriteAnchorPointName(Sprite::AnchorPoint anchorPoint) {
   switch (anchorPoint) {
      case Sprite::AnchorPoint::TopLeft:
         return "TopLeft";
      case Sprite::AnchorPoint::TopCenter:
         return "TopCenter";
      case Sprite::AnchorPoint::TopRight:
         return "TopRight";
      case Sprite::AnchorPoint::MiddleLeft:
         return "MiddleLeft";
      case Sprite::AnchorPoint::MiddleCenter:
         return "MiddleCenter";
      case Sprite::AnchorPoint::MiddleRight:
         return "MiddleRight";
      case Sprite::AnchorPoint::BottomLeft:
         return "BottomLeft";
      case Sprite::AnchorPoint::BottomCenter:
         return "BottomCenter";
      case Sprite::AnchorPoint::BottomRight:
         return "BottomRight";
      default:
         return "MiddleCenter";
   }
}

Sprite::AnchorPoint ParseSpriteAnchorPoint(const nlohmann::json& value, Sprite::AnchorPoint fallback) {
   if (value.is_string()) {
      // 現行形式はenumの並び替えに影響されない名前で保存する。
      const std::string name = value.get<std::string>();
      if (name == "TopLeft") {
         return Sprite::AnchorPoint::TopLeft;
      }
      if (name == "TopCenter") {
         return Sprite::AnchorPoint::TopCenter;
      }
      if (name == "TopRight") {
         return Sprite::AnchorPoint::TopRight;
      }
      if (name == "MiddleLeft") {
         return Sprite::AnchorPoint::MiddleLeft;
      }
      if (name == "MiddleCenter") {
         return Sprite::AnchorPoint::MiddleCenter;
      }
      if (name == "MiddleRight") {
         return Sprite::AnchorPoint::MiddleRight;
      }
      if (name == "BottomLeft") {
         return Sprite::AnchorPoint::BottomLeft;
      }
      if (name == "BottomCenter") {
         return Sprite::AnchorPoint::BottomCenter;
      }
      if (name == "BottomRight") {
         return Sprite::AnchorPoint::BottomRight;
      }
   }

   return fallback;
}
} // namespace

SceneObjectStore::SceneObjectStore() = default;
SceneObjectStore::~SceneObjectStore() = default;

Object* SceneObjectStore::CreateGenericObject(const Transform* initialTransform, const std::string& requestedId) {
   auto object = std::make_unique<Object>();
   Object* rawObject = object.get();
   auto* transformComponent = rawObject->AddComponent<TransformComponent>();
   if (initialTransform && transformComponent) {
      transformComponent->ApplyLocalPose(*initialTransform);
   }
   rawObject->SetObjectName(BuildUniqueObjectName("EmptyObject"));

   const std::string id = AllocateId(requestedId);
   RegisterObject(id, rawObject);
   genericObjects_.push_back(std::move(object));
   return rawObject;
}

Object* SceneObjectStore::CreateModel(const std::string& assetId, const Transform* initialTransform, const std::string& requestedId) {
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

   const std::string id = AllocateId(requestedId);
   RegisterObject(id, rawModel);
   models_.push_back(std::move(model));
   return rawModel;
}

Object* SceneObjectStore::CreateSprite(const std::string& textureAssetId, const Transform* initialTransform, const std::string& requestedId) {
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

   const std::string id = AllocateId(requestedId);
   RegisterObject(id, rawSprite);
   sprites_.push_back(std::move(sprite));
   return rawSprite;
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

   const std::string id = AllocateId(requestedId);
   RegisterObject(id, rawText);
   uiTexts_.push_back(std::move(uiText));
   return rawText;
}

Object* SceneObjectStore::CreateSkybox(const std::string& requestedId) {
   GraphicsDevice* graphicsDevice = EngineContext::GetGraphicsDevice();
   if (!graphicsDevice) {
      return nullptr;
   }

   auto skybox = std::make_unique<Skybox>();
   Skybox* rawSkybox = skybox.get();
   rawSkybox->Create(graphicsDevice);

   const std::string id = AllocateId(requestedId);
   RegisterObject(id, rawSkybox);
   skyboxes_.push_back(std::move(skybox));
   return rawSkybox;
}

ParticleSystem* SceneObjectStore::CreateParticleSystem(const std::string& assetId, const std::string& requestedId, const Transform* initialTransform) {
   auto particleSystem = std::make_unique<ParticleSystem>();
   ParticleSystem* rawParticleSystem = particleSystem.get();
   rawParticleSystem->Create();
   rawParticleSystem->SetName(BuildUniqueObjectName(assetId.empty() ? "ParticleSystem" : BuildNameFromAssetId(assetId)));
   if (!assetId.empty()) {
      if (!rawParticleSystem->LoadFromJson((std::filesystem::path("resources") / assetId).generic_string())) {
         return nullptr;
      }
   }
   // アセット内のShape位置よりユーザーがドロップした配置を優先するため、JSON読込後にTransformを上書きする。
   if (initialTransform && rawParticleSystem->GetShapeModule()) {
      rawParticleSystem->GetShapeModule()->SetTransform(*initialTransform);
   }
   rawParticleSystem->Play();

   const std::string id = AllocateId(requestedId);
   RegisterParticleSystem(id, rawParticleSystem, assetId);
   particleSystems_.push_back(std::move(particleSystem));
   return rawParticleSystem;
}

Object* SceneObjectStore::RestoreObject(const nlohmann::json& objectData) {
   if (!objectData.is_object()) {
      return nullptr;
   }

   const std::string objectType = objectData.value("objectType", "Model");
   // 派生型ごとの必須リソースを先に生成してから、共通コンポーネント状態を上書きする。
   if (objectType == "ParticleSystem") {
      RestoreParticleSystem(objectData);
      return nullptr;
   }

   if (objectType == "Generic" || objectType == "Object") {
      const std::string id = objectData.value("id", "");
      Object* object = CreateGenericObject(nullptr, id);
      if (object) {
         ApplyObjectState(object, objectData);
      }
      return object;
   }

   if (objectType == "UIText") {
      const std::string id = objectData.value("id", "");
      Object* object = CreateUIText(nullptr, id);
      if (!object) {
         return nullptr;
      }

      ApplyObjectState(object, objectData);
      return object;
   }

   if (objectType == "Skybox") {
      const std::string id = objectData.value("id", "");
      Object* object = CreateSkybox(id);
      if (!object) {
         return nullptr;
      }

      ApplyObjectState(object, objectData);
      return object;
   }

   if (objectType == "Sprite") {
      const std::string assetId = objectData.value("assetId", "");
      const std::string id = objectData.value("id", "");
      Object* object = CreateSprite(assetId, nullptr, id);
      auto* sprite = dynamic_cast<Sprite*>(object);
      if (!object || !sprite) {
         return nullptr;
      }

      ApplyObjectState(object, objectData);

      return object;
   }

   if (objectType != "Model") {
      return nullptr;
   }

   const std::string assetId = objectData.value("assetId", "");
   const std::string id = objectData.value("id", "");
   Object* object = CreateModel(assetId, nullptr, id);
   if (!object) {
      return nullptr;
   }

   ApplyObjectState(object, objectData);

   return object;
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
   if (particleSystem) particleSystemAssetIds_[particleSystem] = assetId;
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
   auto mapIt = idToObject_.find(objectId);
   if (mapIt == idToObject_.end()) {
      return false;
   }

   Object* target = mapIt->second;
   // 描画レジストリから先に解除し、フレーム中に残った生ポインターが破棄済みメモリを参照しないようにする。
   if (auto* textTarget = dynamic_cast<UIText*>(target)) {
      auto vecIt = std::find_if(uiTexts_.begin(), uiTexts_.end(),
         [textTarget](const std::unique_ptr<UIText>& text) {
            return text.get() == textTarget;
         });

      if (vecIt == uiTexts_.end()) {
         return false;
      }

      UnregisterObject(target);
      UnregisterOwnedRuntimeSystems(target);
      UIText::UnregisterText(textTarget);
      deferredDeleteUITexts_.push_back(std::move(*vecIt));
      uiTexts_.erase(vecIt);
      return true;
   }

   if (auto* modelTarget = dynamic_cast<Model*>(target)) {
      auto vecIt = std::find_if(models_.begin(), models_.end(),
         [modelTarget](const std::unique_ptr<Model>& model) {
            return model.get() == modelTarget;
         });

      if (vecIt == models_.end()) {
         return false;
      }

      UnregisterObject(target);
      UnregisterOwnedRuntimeSystems(target);
      Model::UnregisterModel(modelTarget);
      deferredDeleteModels_.push_back(std::move(*vecIt));
      models_.erase(vecIt);
      return true;
   }

   if (auto* spriteTarget = dynamic_cast<Sprite*>(target)) {
      auto vecIt = std::find_if(sprites_.begin(), sprites_.end(),
         [spriteTarget](const std::unique_ptr<Sprite>& sprite) {
            return sprite.get() == spriteTarget;
         });

      if (vecIt == sprites_.end()) {
         return false;
      }

      UnregisterObject(target);
      UnregisterOwnedRuntimeSystems(target);
      Sprite::UnregisterSprite(spriteTarget);
      deferredDeleteSprites_.push_back(std::move(*vecIt));
      sprites_.erase(vecIt);
      return true;
   }

   if (auto* skyboxTarget = dynamic_cast<Skybox*>(target)) {
      auto vecIt = std::find_if(skyboxes_.begin(), skyboxes_.end(),
         [skyboxTarget](const std::unique_ptr<Skybox>& skybox) {
            return skybox.get() == skyboxTarget;
         });

      if (vecIt == skyboxes_.end()) {
         return false;
      }

      UnregisterObject(target);
      UnregisterOwnedRuntimeSystems(target);
      Skybox::UnregisterSkybox(skyboxTarget);
      deferredDeleteSkyboxes_.push_back(std::move(*vecIt));
      skyboxes_.erase(vecIt);
      return true;
   }

   auto genericIt = std::find_if(genericObjects_.begin(), genericObjects_.end(),
      [target](const std::unique_ptr<Object>& object) {
         return object.get() == target;
      });
   if (genericIt != genericObjects_.end()) {
      UnregisterObject(target);
      UnregisterOwnedRuntimeSystems(target);
      deferredDeleteGenericObjects_.push_back(std::move(*genericIt));
      genericObjects_.erase(genericIt);
      return true;
   }

   return false;
}

bool SceneObjectStore::DeleteParticleSystem(const std::string& objectId) {
   auto mapIt = idToParticleSystem_.find(objectId);
   if (mapIt == idToParticleSystem_.end()) {
      return false;
   }

   ParticleSystem* target = mapIt->second;
   auto vecIt = std::find_if(particleSystems_.begin(), particleSystems_.end(),
      [target](const std::unique_ptr<ParticleSystem>& particleSystem) {
         return particleSystem.get() == target;
      });

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
   // UI・描画側が前フレームの一覧を使い終えたフレーム先頭で実体を破棄する。
   deferredDeleteGenericObjects_.clear();
   deferredDeleteParticleSystems_.clear();
   deferredDeleteUITexts_.clear();
   deferredDeleteSkyboxes_.clear();
   deferredDeleteSprites_.clear();
   deferredDeleteModels_.clear();
   pendingDeletionObjects_.clear();
}

void SceneObjectStore::Clear() {
   // 各具象型の静的レジストリを解除してからunique_ptrを遅延キューへ移し、列挙中の破棄を避ける。
   for (auto& object : genericObjects_) {
      if (object) {
         UnregisterOwnedRuntimeSystems(object.get());
         deferredDeleteGenericObjects_.push_back(std::move(object));
      }
   }
   for (auto& model : models_) {
      if (model) {
         UnregisterOwnedRuntimeSystems(model.get());
         Model::UnregisterModel(model.get());
         deferredDeleteModels_.push_back(std::move(model));
      }
   }
   for (auto& sprite : sprites_) {
      if (sprite) {
         UnregisterOwnedRuntimeSystems(sprite.get());
         Sprite::UnregisterSprite(sprite.get());
         deferredDeleteSprites_.push_back(std::move(sprite));
      }
   }
   for (auto& uiText : uiTexts_) {
      if (uiText) {
         UnregisterOwnedRuntimeSystems(uiText.get());
         UIText::UnregisterText(uiText.get());
         deferredDeleteUITexts_.push_back(std::move(uiText));
      }
   }
   for (auto& skybox : skyboxes_) {
      if (skybox) {
         UnregisterOwnedRuntimeSystems(skybox.get());
         Skybox::UnregisterSkybox(skybox.get());
         deferredDeleteSkyboxes_.push_back(std::move(skybox));
      }
   }
   for (auto& particleSystem : particleSystems_) {
      if (particleSystem) {
         ParticleSystem::UnregisterParticleSystem(particleSystem.get());
         deferredDeleteParticleSystems_.push_back(std::move(particleSystem));
      }
   }

   objectToId_.clear();
   idToObject_.clear();
   particleSystemToId_.clear();
   idToParticleSystem_.clear();
   particleSystemAssetIds_.clear();
   particleSystems_.clear();
   skyboxes_.clear();
   uiTexts_.clear();
   sprites_.clear();
   models_.clear();
   genericObjects_.clear();
}

bool SceneObjectStore::Contains(const Object* object) const {
   return object && objectToId_.contains(object);
}

bool SceneObjectStore::Contains(const ParticleSystem* particleSystem) const {
   return particleSystem && particleSystemToId_.contains(particleSystem);
}

bool SceneObjectStore::ContainsId(const std::string& objectId) const {
   // BaseScene所有EntityともID空間を共有するため、Store外のグローバルEntity IDまで衝突判定へ含める。
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

   const std::string stableId = id.empty() ? object->GetEntityId() : id;
   const std::string& parentId = object->GetParentEntityId();

   if (const auto* uiText = dynamic_cast<const UIText*>(object)) {
      // 復元前に正しい具象型を生成できるよう、Object共通データとは別に安定した種別名を保存する。
      return nlohmann::json{
         { "id", stableId },
         { "parentId", parentId },
         { "objectType", "UIText" },
         { "components", uiText->SerializeComponents() }
      };
   }

   if (const auto* sprite = dynamic_cast<const Sprite*>(object)) {
      std::string textureAssetId;
      if (const auto* materialComponent = sprite->GetComponent<MaterialComponent>()) {
         textureAssetId = materialComponent->GetTextureName();
      }

      return nlohmann::json{
         { "id", stableId },
         { "parentId", parentId },
         { "objectType", "Sprite" },
         { "assetId", textureAssetId },
         { "components", sprite->SerializeComponents() },
         { "sprite", SerializeSpriteData(sprite) }
      };
   }

   if (const auto* skybox = dynamic_cast<const Skybox*>(object)) {
      return nlohmann::json{
         { "id", stableId },
         { "parentId", parentId },
         { "objectType", "Skybox" },
         { "components", skybox->SerializeComponents() }
      };
   }

   const auto* model = dynamic_cast<const Model*>(object);
   if (!model) {
      return nlohmann::json{
         { "id", stableId },
         { "parentId", parentId },
         { "objectType", "Generic" },
         { "components", object->SerializeComponents() }
      };
   }

   std::string assetId;
   if (const auto* meshComponent = model->GetComponent<MeshComponent>()) {
      if (meshComponent->GetSourceType() == MeshComponent::SourceType::ModelFile) {
         assetId = meshComponent->GetAssetId();
      }
   }

   return nlohmann::json{
      { "id", stableId },
      { "parentId", parentId },
      { "objectType", "Model" },
      { "assetId", assetId },
      { "components", model->SerializeComponents() }
   };
}

bool SceneObjectStore::ApplyObjectState(Object* object, const nlohmann::json& objectData) const {
   if (!object || !objectData.is_object()) {
      return false;
   }

   const std::string objectType = objectData.value("objectType", "Object");
   // 異なる具象型へスナップショットを適用すると専用データが欠落するため、先に型整合性を検証する。
   if (objectType == "Model" && dynamic_cast<Model*>(object) == nullptr) {
      return false;
   }
   if (objectType == "Sprite" && dynamic_cast<Sprite*>(object) == nullptr) {
      return false;
   }
   if (objectType == "UIText" && dynamic_cast<UIText*>(object) == nullptr) {
      return false;
   }
   if (objectType == "Skybox" && dynamic_cast<Skybox*>(object) == nullptr) {
      return false;
   }

   if (objectData.contains("components") && objectData.at("components").is_array()) {
      object->DeserializeComponents(objectData.at("components"));
   }
   if (objectData.contains("parentId") && objectData.at("parentId").is_string()) {
      object->SetParentEntityId(objectData.at("parentId").get<std::string>());
   }

   if (auto* sprite = dynamic_cast<Sprite*>(object)) {
      if (objectData.contains("sprite") && objectData.at("sprite").is_object()) {
         DeserializeSpriteData(sprite, objectData.at("sprite"));
      }
   }

   return true;
}

nlohmann::json SceneObjectStore::SerializeParticleSystemState(const ParticleSystem* particleSystem, const std::string& id, const std::string& assetId) const {
   if (!particleSystem) {
      return nlohmann::json::object();
   }

   return nlohmann::json{
      { "id", id },
      { "objectType", "ParticleSystem" },
      { "name", particleSystem->GetName() },
      { "assetId", assetId },
      { "data", particleSystem->ToJson() }
   };
}

bool SceneObjectStore::ApplyParticleSystemState(ParticleSystem* particleSystem, const nlohmann::json& objectData) const {
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
   nlohmann::json objects = nlohmann::json::array();
   // 所有コンテナは具象型ごとに分かれているが、復元側がobjectTypeで振り分けられる単一配列へ正規化する。
   for (const auto& object : genericObjects_) {
      if (!object) {
         continue;
      }

      const std::string id = GetId(object.get());
      if (!id.empty()) {
         objects.push_back(SerializeObject(id));
      }
   }
   for (const auto& model : models_) {
      if (!model) {
         continue;
      }

      const std::string id = GetId(model.get());
      if (id.empty()) {
         continue;
      }

      objects.push_back(SerializeObject(id));
   }

   for (const auto& sprite : sprites_) {
      if (!sprite) {
         continue;
      }

      const std::string id = GetId(sprite.get());
      if (id.empty()) {
         continue;
      }

      objects.push_back(SerializeObject(id));
   }

   for (const auto& uiText : uiTexts_) {
      if (!uiText) {
         continue;
      }

      const std::string id = GetId(uiText.get());
      if (id.empty()) {
         continue;
      }

      objects.push_back(SerializeObject(id));
   }

   for (const auto& skybox : skyboxes_) {
      if (!skybox) {
         continue;
      }

      const std::string id = GetId(skybox.get());
      if (id.empty()) {
         continue;
      }

      objects.push_back(SerializeObject(id));
   }

   for (const auto& particleSystem : particleSystems_) {
      if (!particleSystem) {
         continue;
      }

      const std::string id = GetId(particleSystem.get());
      if (id.empty()) {
         continue;
      }

      objects.push_back(SerializeObject(id));
   }
   return objects;
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

   int suffix = 1;
   while (true) {
      const std::string candidate = base + "_" + std::to_string(suffix++);
      if (!exists(candidate)) {
         return candidate;
      }
   }
}

void SceneObjectStore::RegisterObject(const std::string& id, Object* object) {
   if (!object || id.empty()) {
      return;
   }

   // ID検索とポインター検索の双方向表を同時に更新し、Undoコマンドのどちらの参照経路も一致させる。
   idToObject_[id] = object;
   objectToId_[object] = id;
   object->SetEntityId(id);
}

void SceneObjectStore::RegisterParticleSystem(const std::string& id, ParticleSystem* particleSystem, const std::string& assetId) {
   if (!particleSystem || id.empty()) {
      return;
   }

   // 再保存時に元テンプレートも保持できるよう、識別表とassetIdを同じ寿命で登録する。
   idToParticleSystem_[id] = particleSystem;
   particleSystemToId_[particleSystem] = id;
   particleSystemAssetIds_[particleSystem] = assetId;
}

void SceneObjectStore::UnregisterObject(Object* object) {
   if (!object) {
      return;
   }

   auto objectIt = objectToId_.find(object);
   if (objectIt != objectToId_.end()) {
      const std::string oldId = objectIt->second;
      idToObject_.erase(oldId);
      objectToId_.erase(objectIt);
      // 実体は描画フレーム末まで残すが、Undoは同じ安定IDを即座に復元できる必要がある。
      // 子は先にルートへ戻し、一時IDの伝播や旧実体の破棄による再解除を防ぐ。
      for (auto* child : Object::GetRegisteredObjects()) {
         if (child && child != object && child->GetParentEntityId() == oldId) child->SetParentEntityId({});
      }
      std::string retiredId;
      do {
         retiredId = "pending_delete_" + oldId + "_" + std::to_string(nextObjectIndex_++);
      } while (Object::FindByEntityId(retiredId));
      object->SetEntityId(retiredId);
   }
}

void SceneObjectStore::UnregisterParticleSystem(ParticleSystem* particleSystem) {
   if (!particleSystem) {
      return;
   }

   auto objectIt = particleSystemToId_.find(particleSystem);
   if (objectIt != particleSystemToId_.end()) {
      idToParticleSystem_.erase(objectIt->second);
      particleSystemToId_.erase(objectIt);
   }
   particleSystemAssetIds_.erase(particleSystem);
}

void SceneObjectStore::UnregisterOwnedRuntimeSystems(Object* object) {
   if (!object) {
      return;
   }

   pendingDeletionObjects_.insert(object);
   // 親Objectより長く静的描画リストへ残らないよう、Emitterが所有する実行時システムも先に解除する。
   if (auto* emitter = object->GetComponent<ParticleEmitterComponent>()) {
      emitter->UnregisterParticleSystemsForRender();
   }
}

void SceneObjectStore::BumpCounterFromId(const std::string& id) {
   constexpr const char* kPrefix = "editor_object_";
   const std::string prefix = kPrefix;
   if (id.rfind(prefix, 0) != 0) {
      return;
   }

   const std::string numberText = id.substr(prefix.size());
   if (numberText.empty()) {
      return;
   }

   try {
      const uint64_t number = std::stoull(numberText);
      nextObjectIndex_ = std::max(nextObjectIndex_, number + 1);
   } catch (...) {
      // 接頭辞が同じでも数値でない外部IDは有効な識別子として残し、自動採番だけを変更しない。
   }
}

nlohmann::json SceneObjectStore::SerializeSpriteData(const Sprite* sprite) {
   if (!sprite) {
      return nlohmann::json::object();
   }

   return nlohmann::json{
      { "screenAnchorPoint", ToSpriteAnchorPointName(sprite->GetScreenAnchorPoint()) }
   };
}

void SceneObjectStore::DeserializeSpriteData(Sprite* sprite, const nlohmann::json& data) const {
   if (!sprite || !data.is_object()) {
      return;
   }

   // Sprite専用状態はスクリーンアンカーだけで、寸法・反転・UVはComponentが復元する。
   if (data.contains("screenAnchorPoint")) {
      sprite->SetScreenAnchorPoint(ParseSpriteAnchorPoint(data.at("screenAnchorPoint"), sprite->GetScreenAnchorPoint()));
   }
}

} // namespace GameEngine
