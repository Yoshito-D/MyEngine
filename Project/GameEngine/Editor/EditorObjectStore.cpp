#include "pch.h"
#include "EditorObjectStore.h"

#ifdef USE_IMGUI

#include "Component/ModelAssetComponent.h"
#include "Component/MaterialComponent.h"
#include "Component/ParticleEmitterComponent.h"
#include "Component/TransformComponent.h"
#include "Effect/ParticleSystem.h"
#include "Framework/EngineContext.h"
#include "Model/Model.h"
#include "Sprite/Sprite.h"
#include <algorithm>
#include <filesystem>

namespace GameEngine {

namespace {
std::string BuildNameFromAssetId(const std::string& assetId) {
   if (assetId.empty()) {
      return "EditorModel";
   }
   return std::filesystem::path(assetId).stem().string();
}
} // namespace

Object* EditorObjectStore::CreateModel(const std::string& assetId, const Transform* initialTransform, const std::string& requestedId) {
   auto modelAsset = EngineContext::LoadModelByAssetId(assetId);
   if (!modelAsset) {
      return nullptr;
   }

   auto model = std::make_unique<Model>();
   Model* rawModel = model.get();
   rawModel->Create().SetModelAsset(modelAsset);
   rawModel->SetObjectName(BuildUniqueObjectName(BuildNameFromAssetId(assetId)));

   if (initialTransform) {
      rawModel->SetTransform(*initialTransform);
   }

   const std::string id = AllocateId(requestedId);
   RegisterObject(id, rawModel);
   models_.push_back(std::move(model));
   return rawModel;
}

Object* EditorObjectStore::CreateSprite(const std::string& textureAssetId, const Transform* initialTransform, const std::string& requestedId) {
   if (textureAssetId.empty()) {
      return nullptr;
   }

   EnsureTextureLoaded(textureAssetId);

   Vector2 spriteSize(128.0f, 128.0f);
   if (Texture* texture = EngineContext::GetTexture(textureAssetId)) {
      if (texture->GetMetadata().IsCubemap()) {
         return nullptr;
      }
      spriteSize = Vector2(static_cast<float>(texture->GetWidth()), static_cast<float>(texture->GetHeight()));
   }

   auto sprite = std::make_unique<Sprite>();
   Sprite* rawSprite = sprite.get();
   rawSprite->Create(spriteSize, nullptr, Vector2(0.5f, 0.5f));
   rawSprite->SetObjectName(BuildUniqueObjectName(BuildNameFromAssetId(textureAssetId)));

   if (auto* materialComponent = rawSprite->GetComponent<MaterialComponent>()) {
      materialComponent->SetTextureName(textureAssetId);
   }

   if (initialTransform) {
      if (auto* transformComponent = rawSprite->GetComponent<TransformComponent>()) {
         transformComponent->transform = *initialTransform;
      }
   }

   const std::string id = AllocateId(requestedId);
   RegisterObject(id, rawSprite);
   sprites_.push_back(std::move(sprite));
   return rawSprite;
}

ParticleSystem* EditorObjectStore::CreateParticleSystem(const std::string& assetId, const std::string& requestedId, const Transform* initialTransform) {
   auto particleSystem = std::make_unique<ParticleSystem>();
   ParticleSystem* rawParticleSystem = particleSystem.get();
   rawParticleSystem->Create();
   rawParticleSystem->SetName(BuildUniqueObjectName(assetId.empty() ? "ParticleSystem" : BuildNameFromAssetId(assetId)));
   if (!assetId.empty()) {
      rawParticleSystem->LoadFromJson((std::filesystem::path("resources") / assetId).generic_string());
   }
   if (initialTransform && rawParticleSystem->GetShapeModule()) {
      rawParticleSystem->GetShapeModule()->SetTransform(*initialTransform);
   }
   rawParticleSystem->Play();

   const std::string id = AllocateId(requestedId);
   RegisterParticleSystem(id, rawParticleSystem, assetId);
   particleSystems_.push_back(std::move(particleSystem));
   return rawParticleSystem;
}

Object* EditorObjectStore::RestoreObject(const nlohmann::json& objectData) {
   if (!objectData.is_object()) {
      return nullptr;
   }

   const std::string objectType = objectData.value("objectType", "Model");
   if (objectType == "ParticleSystem") {
      RestoreParticleSystem(objectData);
      return nullptr;
   }

   if (objectType == "Sprite") {
      const std::string assetId = objectData.value("assetId", "");
      const std::string id = objectData.value("id", "");
      Object* object = CreateSprite(assetId, nullptr, id);
      auto* sprite = dynamic_cast<Sprite*>(object);
      if (!object || !sprite) {
         return nullptr;
      }

      if (objectData.contains("components") && objectData.at("components").is_array()) {
         object->DeserializeComponents(objectData.at("components"));
      }

      if (objectData.contains("sprite") && objectData.at("sprite").is_object()) {
         DeserializeSpriteData(sprite, objectData.at("sprite"));
      }

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

   if (objectData.contains("components") && objectData.at("components").is_array()) {
      object->DeserializeComponents(objectData.at("components"));
   }

   return object;
}

ParticleSystem* EditorObjectStore::RestoreParticleSystem(const nlohmann::json& objectData) {
   if (!objectData.is_object()) {
      return nullptr;
   }

   const std::string assetId = objectData.value("assetId", "");
   const std::string id = objectData.value("id", "");
   ParticleSystem* particleSystem = CreateParticleSystem(assetId, id);
   if (particleSystem && objectData.contains("data") && objectData.at("data").is_object()) {
      particleSystem->FromJson(objectData.at("data"));
   }
   if (particleSystem && objectData.contains("name") && objectData.at("name").is_string()) {
      particleSystem->SetName(objectData.at("name").get<std::string>());
   }
   return particleSystem;
}

bool EditorObjectStore::DeleteObject(const std::string& objectId) {
   auto mapIt = idToObject_.find(objectId);
   if (mapIt == idToObject_.end()) {
      return false;
   }

   Object* target = mapIt->second;
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

   return false;
}

bool EditorObjectStore::DeleteParticleSystem(const std::string& objectId) {
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

   UnregisterParticleSystem(target);
   ParticleSystem::UnregisterParticleSystem(target);
   deferredDeleteParticleSystems_.push_back(std::move(*vecIt));
   particleSystems_.erase(vecIt);
   return true;
}

void EditorObjectStore::FlushDeferredDeletes() {
   deferredDeleteParticleSystems_.clear();
   deferredDeleteSprites_.clear();
   deferredDeleteModels_.clear();
}

void EditorObjectStore::Clear() {
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
   sprites_.clear();
   models_.clear();
}

bool EditorObjectStore::Contains(const Object* object) const {
   return object && objectToId_.contains(object);
}

bool EditorObjectStore::Contains(const ParticleSystem* particleSystem) const {
   return particleSystem && particleSystemToId_.contains(particleSystem);
}

bool EditorObjectStore::ContainsId(const std::string& objectId) const {
   return idToObject_.contains(objectId) || idToParticleSystem_.contains(objectId);
}

std::string EditorObjectStore::GetId(const Object* object) const {
   auto it = objectToId_.find(object);
   if (it == objectToId_.end()) {
      return {};
   }
   return it->second;
}

std::string EditorObjectStore::GetId(const ParticleSystem* particleSystem) const {
   auto it = particleSystemToId_.find(particleSystem);
   if (it == particleSystemToId_.end()) {
      return {};
   }
   return it->second;
}

Object* EditorObjectStore::FindById(const std::string& objectId) const {
   auto it = idToObject_.find(objectId);
   if (it == idToObject_.end()) {
      return nullptr;
   }
   return it->second;
}

ParticleSystem* EditorObjectStore::FindParticleById(const std::string& objectId) const {
   auto it = idToParticleSystem_.find(objectId);
   if (it == idToParticleSystem_.end()) {
      return nullptr;
   }
   return it->second;
}

nlohmann::json EditorObjectStore::SerializeObject(const std::string& objectId) const {
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

nlohmann::json EditorObjectStore::SerializeObjectState(const Object* object, const std::string& id) const {
   if (!object) {
      return nlohmann::json::object();
   }

   if (const auto* sprite = dynamic_cast<const Sprite*>(object)) {
      std::string textureAssetId;
      if (const auto* materialComponent = sprite->GetComponent<MaterialComponent>()) {
         textureAssetId = materialComponent->GetTextureName();
      }

      return nlohmann::json{
         { "id", id },
         { "objectType", "Sprite" },
         { "assetId", textureAssetId },
         { "components", sprite->SerializeComponents() },
         { "sprite", SerializeSpriteData(sprite) }
      };
   }

   const auto* model = dynamic_cast<const Model*>(object);
   if (!model) {
      return nlohmann::json{
         { "id", id },
         { "objectType", "Object" },
         { "components", object->SerializeComponents() }
      };
   }

   std::string assetId;
   if (const auto* modelAssetComponent = model->GetComponent<ModelAssetComponent>()) {
      assetId = modelAssetComponent->GetAssetId();
   }

   return nlohmann::json{
      { "id", id },
      { "objectType", "Model" },
      { "assetId", assetId },
      { "components", model->SerializeComponents() }
   };
}

bool EditorObjectStore::ApplyObjectState(Object* object, const nlohmann::json& objectData) const {
   if (!object || !objectData.is_object()) {
      return false;
   }

   const std::string objectType = objectData.value("objectType", "Object");
   if (objectType == "Model" && dynamic_cast<Model*>(object) == nullptr) {
      return false;
   }
   if (objectType == "Sprite" && dynamic_cast<Sprite*>(object) == nullptr) {
      return false;
   }

   if (objectData.contains("components") && objectData.at("components").is_array()) {
      object->DeserializeComponents(objectData.at("components"));
   }

   if (auto* sprite = dynamic_cast<Sprite*>(object)) {
      if (objectData.contains("sprite") && objectData.at("sprite").is_object()) {
         DeserializeSpriteData(sprite, objectData.at("sprite"));
      }
   }

   return true;
}

nlohmann::json EditorObjectStore::SerializeParticleSystemState(const ParticleSystem* particleSystem, const std::string& id, const std::string& assetId) const {
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

bool EditorObjectStore::ApplyParticleSystemState(ParticleSystem* particleSystem, const nlohmann::json& objectData) const {
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

nlohmann::json EditorObjectStore::SerializeAll() const {
   nlohmann::json objects = nlohmann::json::array();
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

std::string EditorObjectStore::AllocateId(const std::string& requestedId) {
   if (!requestedId.empty() && !idToObject_.contains(requestedId) && !idToParticleSystem_.contains(requestedId)) {
      BumpCounterFromId(requestedId);
      return requestedId;
   }

   while (true) {
      const std::string id = "editor_object_" + std::to_string(nextObjectIndex_++);
      if (!idToObject_.contains(id) && !idToParticleSystem_.contains(id)) {
         return id;
      }
   }
}

std::string EditorObjectStore::BuildUniqueObjectName(const std::string& baseName) const {
   const std::string base = baseName.empty() ? "EditorObject" : baseName;

   auto exists = [](const std::string& name) {
      for (const auto* model : Model::GetRegisteredModels()) {
         if (model && model->GetObjectName() == name) {
            return true;
         }
      }
      for (const auto* sprite : Sprite::GetRegisteredSprites()) {
         if (sprite && sprite->GetObjectName() == name) {
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

void EditorObjectStore::RegisterObject(const std::string& id, Object* object) {
   if (!object || id.empty()) {
      return;
   }

   idToObject_[id] = object;
   objectToId_[object] = id;
}

void EditorObjectStore::RegisterParticleSystem(const std::string& id, ParticleSystem* particleSystem, const std::string& assetId) {
   if (!particleSystem || id.empty()) {
      return;
   }

   idToParticleSystem_[id] = particleSystem;
   particleSystemToId_[particleSystem] = id;
   particleSystemAssetIds_[particleSystem] = assetId;
}

void EditorObjectStore::UnregisterObject(Object* object) {
   if (!object) {
      return;
   }

   auto objectIt = objectToId_.find(object);
   if (objectIt != objectToId_.end()) {
      idToObject_.erase(objectIt->second);
      objectToId_.erase(objectIt);
   }
}

void EditorObjectStore::UnregisterParticleSystem(ParticleSystem* particleSystem) {
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

void EditorObjectStore::UnregisterOwnedRuntimeSystems(Object* object) {
   if (!object) {
      return;
   }

   if (auto* emitter = object->GetComponent<ParticleEmitterComponent>()) {
      emitter->UnregisterParticleSystemsForRender();
   }
}

void EditorObjectStore::BumpCounterFromId(const std::string& id) {
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
   }
}

nlohmann::json EditorObjectStore::SerializeSpriteData(const Sprite* sprite) {
   if (!sprite) {
      return nlohmann::json::object();
   }

   const Vector2 size = sprite->GetSize();
   const Vector2 anchor = sprite->GetAnchorPoint();
   const Vector2 uvLeftTop = sprite->GetTextureLeftTop();
   const Vector2 uvSize = sprite->GetTextureSize();

   return nlohmann::json{
      { "size", { size.x, size.y } },
      { "anchor", { anchor.x, anchor.y } },
      { "flipX", sprite->IsFlipX() },
      { "flipY", sprite->IsFlipY() },
      { "textureLeftTop", { uvLeftTop.x, uvLeftTop.y } },
      { "textureSize", { uvSize.x, uvSize.y } }
   };
}

void EditorObjectStore::DeserializeSpriteData(Sprite* sprite, const nlohmann::json& data) const {
   if (!sprite || !data.is_object()) {
      return;
   }

   if (data.contains("size") && data.at("size").is_array() && data.at("size").size() == 2) {
      sprite->SetSize(Vector2(data.at("size")[0].get<float>(), data.at("size")[1].get<float>()));
   }

   if (data.contains("anchor") && data.at("anchor").is_array() && data.at("anchor").size() == 2) {
      sprite->SetAnchorPoint(Vector2(data.at("anchor")[0].get<float>(), data.at("anchor")[1].get<float>()));
   }

   if (data.contains("flipX") && data.at("flipX").is_boolean()) {
      sprite->SetFlipX(data.at("flipX").get<bool>());
   }
   if (data.contains("flipY") && data.at("flipY").is_boolean()) {
      sprite->SetFlipY(data.at("flipY").get<bool>());
   }

   if (data.contains("textureLeftTop") && data.at("textureLeftTop").is_array() && data.at("textureLeftTop").size() == 2) {
      sprite->SetTextureLeftTop(Vector2(data.at("textureLeftTop")[0].get<float>(), data.at("textureLeftTop")[1].get<float>()));
   }

   if (data.contains("textureSize") && data.at("textureSize").is_array() && data.at("textureSize").size() == 2) {
      sprite->SetTextureSize(Vector2(data.at("textureSize")[0].get<float>(), data.at("textureSize")[1].get<float>()));
   }
}

bool EditorObjectStore::EnsureTextureLoaded(const std::string& textureAssetId) const {
   if (textureAssetId.empty()) {
      return false;
   }

   if (EngineContext::GetTexture(textureAssetId)) {
      return true;
   }

   const std::filesystem::path texturePath(textureAssetId);
   return EngineContext::GetTexture(texturePath.stem().string()) != nullptr ||
      EngineContext::GetTexture(texturePath.filename().string()) != nullptr;
}

} // namespace GameEngine

#endif
