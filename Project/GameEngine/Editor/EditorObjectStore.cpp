#include "pch.h"
#include "EditorObjectStore.h"

#ifdef USE_IMGUI

#include "Component/ModelAssetComponent.h"
#include "Component/TransformComponent.h"
#include "Effect/ParticleSystem.h"
#include "Framework/EngineContext.h"
#include "Model/Model.h"
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

ParticleSystem* EditorObjectStore::CreateParticleSystem(const std::string& assetId, const std::string& requestedId) {
   if (assetId.empty()) {
      return nullptr;
   }

   auto particleSystem = std::make_unique<ParticleSystem>();
   ParticleSystem* rawParticleSystem = particleSystem.get();
   rawParticleSystem->Create();
   rawParticleSystem->SetName(BuildUniqueObjectName(BuildNameFromAssetId(assetId)));
   rawParticleSystem->LoadFromJson((std::filesystem::path("resources") / assetId).generic_string());
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
      const std::string assetId = objectData.value("assetId", "");
      const std::string id = objectData.value("id", "");
      ParticleSystem* particleSystem = CreateParticleSystem(assetId, id);
      if (particleSystem && objectData.contains("data") && objectData.at("data").is_object()) {
         particleSystem->FromJson(objectData.at("data"));
      }
      return nullptr;
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

bool EditorObjectStore::DeleteObject(const std::string& objectId) {
   auto mapIt = idToModel_.find(objectId);
   if (mapIt == idToModel_.end()) {
      return false;
   }

   Model* target = mapIt->second;
   auto vecIt = std::find_if(models_.begin(), models_.end(),
      [target](const std::unique_ptr<Model>& model) {
         return model.get() == target;
      });

   if (vecIt == models_.end()) {
      return false;
   }

   UnregisterObject(target);
   models_.erase(vecIt);
   return true;
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
   particleSystems_.erase(vecIt);
   return true;
}

void EditorObjectStore::Clear() {
   objectToId_.clear();
   idToModel_.clear();
   particleSystemToId_.clear();
   idToParticleSystem_.clear();
   particleSystemAssetIds_.clear();
   particleSystems_.clear();
   models_.clear();
}

bool EditorObjectStore::Contains(const Object* object) const {
   return object && objectToId_.contains(object);
}

bool EditorObjectStore::Contains(const ParticleSystem* particleSystem) const {
   return particleSystem && particleSystemToId_.contains(particleSystem);
}

bool EditorObjectStore::ContainsId(const std::string& objectId) const {
   return idToModel_.contains(objectId) || idToParticleSystem_.contains(objectId);
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
   auto it = idToModel_.find(objectId);
   if (it == idToModel_.end()) {
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
   auto modelIt = idToModel_.find(objectId);
   if (modelIt == idToModel_.end()) {
      auto particleIt = idToParticleSystem_.find(objectId);
      if (particleIt == idToParticleSystem_.end() || !particleIt->second) {
         return nlohmann::json::object();
      }

      const ParticleSystem* particleSystem = particleIt->second;
      std::string assetId;
      if (auto assetIt = particleSystemAssetIds_.find(particleSystem); assetIt != particleSystemAssetIds_.end()) {
         assetId = assetIt->second;
      }

      return nlohmann::json{
         { "id", objectId },
         { "objectType", "ParticleSystem" },
         { "assetId", assetId },
         { "data", particleSystem->ToJson() }
      };
   }

   if (!modelIt->second) {
      return nlohmann::json::object();
   }

   const Model* model = modelIt->second;
   std::string assetId;
   if (const auto* modelAssetComponent = model->GetComponent<ModelAssetComponent>()) {
      assetId = modelAssetComponent->GetAssetId();
   }

   return nlohmann::json{
      { "id", objectId },
      { "objectType", "Model" },
      { "assetId", assetId },
      { "components", model->SerializeComponents() }
   };
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
   if (!requestedId.empty() && !idToModel_.contains(requestedId)) {
      BumpCounterFromId(requestedId);
      return requestedId;
   }

   while (true) {
      const std::string id = "editor_object_" + std::to_string(nextObjectIndex_++);
      if (!idToModel_.contains(id)) {
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

void EditorObjectStore::RegisterObject(const std::string& id, Model* model) {
   if (!model || id.empty()) {
      return;
   }

   idToModel_[id] = model;
   objectToId_[model] = id;
}

void EditorObjectStore::RegisterParticleSystem(const std::string& id, ParticleSystem* particleSystem, const std::string& assetId) {
   if (!particleSystem || id.empty()) {
      return;
   }

   idToParticleSystem_[id] = particleSystem;
   particleSystemToId_[particleSystem] = id;
   particleSystemAssetIds_[particleSystem] = assetId;
}

void EditorObjectStore::UnregisterObject(Model* model) {
   if (!model) {
      return;
   }

   auto objectIt = objectToId_.find(model);
   if (objectIt != objectToId_.end()) {
      idToModel_.erase(objectIt->second);
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

} // namespace GameEngine

#endif
