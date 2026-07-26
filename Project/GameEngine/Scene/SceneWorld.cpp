#include "pch.h"
#include "SceneWorld.h"

#include "Component/TransformComponent.h"
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
   Clear();
   if (!sceneData.is_object()) {
      Logger::Error("SceneWorld load failed: scene root is not an object");
      return false;
   }

   sCurrent_ = this;
   bool loadedAnyObject = false;
   if (sceneData.contains("objects") && sceneData.at("objects").is_array()) {
      for (const auto& objectData : sceneData.at("objects")) {
         loadedAnyObject = RestoreObjectEntry(objectData) || loadedAnyObject;
      }
   }

   // version 3までのsceneObjectsはC++生成物への差分だった。
   // データ駆動シーンでは埋め込まれた完全スナップショットを通常オブジェクトとして復元する。
   RestoreLegacyEntries(sceneData);
   RestoreCameras(sceneData);
   if (sceneData.contains("environment") &&
      !EngineContext::ApplyLightingSceneState(sceneData.at("environment"))) {
      Logger::Error("SceneWorld load failed: invalid environment settings");
      return false;
   }
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
   if (auto* brain = EngineContext::GetActiveBrain()) {
      for (const auto& camera : virtualCameras_) {
         if (camera) {
            brain->UnregisterVirtualCamera(camera.get());
         }
      }
   }

   virtualCamerasById_.clear();
   virtualCameras_.clear();
   looseObjectIds_.clear();
   looseObjectsById_.clear();
   skyboxes_.clear();
   genericObjects_.clear();
   objectStore_.Clear();
   objectStore_.FlushDeferredDeletes();
   cameraData_ = nlohmann::json::object();

   if (sCurrent_ == this) {
      sCurrent_ = nullptr;
   }
}

void SceneWorld::Update(float deltaTime) {
   for (const auto& object : genericObjects_) {
      if (object) {
         object->UpdateComponents(deltaTime);
      }
   }
   for (const auto& skybox : skyboxes_) {
      if (skybox) {
         skybox->UpdateComponents(deltaTime);
      }
   }
}

Object* SceneWorld::FindObjectById(const std::string& objectId) const {
   if (objectId.empty()) {
      return nullptr;
   }
   if (auto it = looseObjectsById_.find(objectId); it != looseObjectsById_.end()) {
      return it->second;
   }
   return objectStore_.FindById(objectId);
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

   nlohmann::json objectData = sourceData;
   std::string objectType = objectData.value("objectType", "Model");
   std::string id = objectData.value("id", legacySceneKey);
   if (id.empty()) {
      Logger::EngineWarning("Scene object skipped because it has no stable id");
      return false;
   }
   if (FindObjectById(id) || objectStore_.FindParticleById(id)) {
      Logger::EngineWarning("Duplicate scene object id skipped: " + id);
      return false;
   }

   if ((objectType == "Object" || objectType == "Generic") && legacySceneKey.rfind("Skybox:", 0) == 0) {
      objectType = "Skybox";
      objectData["objectType"] = objectType;
   }

   if (objectType == "Generic" || objectType == "Object") {
      auto object = std::make_unique<Object>();
      Object* rawObject = object.get();
      rawObject->SetObjectName(id);
      if (objectData.contains("components") && objectData.at("components").is_array()) {
         rawObject->DeserializeComponents(objectData.at("components"));
      }
      RegisterLooseObject(id, rawObject);
      genericObjects_.push_back(std::move(object));
      return true;
   }

   if (objectType == "Skybox") {
      auto skybox = std::make_unique<Skybox>();
      Skybox* rawSkybox = skybox.get();
      rawSkybox->Create(EngineContext::GetGraphicsDevice());
      rawSkybox->SetObjectName(id);
      if (objectData.contains("components") && objectData.at("components").is_array()) {
         rawSkybox->DeserializeComponents(objectData.at("components"));
      }
      RegisterLooseObject(id, rawSkybox);
      skyboxes_.push_back(std::move(skybox));
      return true;
   }

   if (objectType == "ParticleSystem") {
      return objectStore_.RestoreParticleSystem(objectData) != nullptr;
   }

   return objectStore_.RestoreObject(objectData) != nullptr;
}

void SceneWorld::RestoreLegacyEntries(const nlohmann::json& sceneData) {
   if (sceneData.contains("sceneObjects") && sceneData.at("sceneObjects").is_array()) {
      for (const auto& entry : sceneData.at("sceneObjects")) {
         if (!entry.is_object() || entry.value("deleted", false) ||
            !entry.contains("object") || !entry.at("object").is_object()) {
            continue;
         }
         RestoreObjectEntry(entry.at("object"), entry.value("sceneKey", ""));
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

void SceneWorld::RestoreCameras(const nlohmann::json& sceneData) {
   if (!sceneData.contains("cameras") || !sceneData.at("cameras").is_object()) {
      return;
   }

   cameraData_ = sceneData.at("cameras");
   auto* brain = EngineContext::GetActiveBrain();
   if (!brain) {
      Logger::EngineWarning("Virtual cameras skipped because no active camera brain exists");
      return;
   }

   if (cameraData_.contains("brain") && cameraData_.at("brain").is_object()) {
      const auto& brainData = cameraData_.at("brain");
      if (brainData.contains("defaultBlendTime") && brainData.at("defaultBlendTime").is_number()) {
         brain->SetDefaultBlendTime(brainData.at("defaultBlendTime").get<float>());
      }
   }

   if (!cameraData_.contains("virtualCameras") || !cameraData_.at("virtualCameras").is_array()) {
      return;
   }

   for (const auto& cameraData : cameraData_.at("virtualCameras")) {
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
      brain->RegisterVirtualCamera(rawCamera);
      virtualCamerasById_[id] = rawCamera;
      virtualCameras_.push_back(std::move(camera));
   }
}

void SceneWorld::ResolveReferences() {
   if (cameraData_.contains("virtualCameras") && cameraData_.at("virtualCameras").is_array()) {
      for (const auto& cameraData : cameraData_.at("virtualCameras")) {
         if (!cameraData.is_object()) {
            continue;
         }
         const std::string cameraKey = cameraData.value("id", cameraData.value("name", ""));
         VirtualCamera* camera = FindVirtualCamera(cameraKey);
         if (!camera) {
            continue;
         }

         auto resolveTransform = [this](const nlohmann::json& data, const char* key) -> Transform* {
            if (!data.contains(key) || !data.at(key).is_string()) {
               return nullptr;
            }
            Object* object = FindObjectById(data.at(key).get<std::string>());
            if (!object) {
               return nullptr;
            }
            auto* transform = object->GetComponent<TransformComponent>();
            return transform ? &transform->transform : nullptr;
         };
         camera->SetFollowTarget(resolveTransform(cameraData, "followTargetId"));
         camera->SetLookAtTarget(resolveTransform(cameraData, "lookAtTargetId"));
      }
   }

   for (Object* object : CollectObjects()) {
      if (!object) {
         continue;
      }
      for (const auto& component : object->GetComponentContainer().GetAll()) {
         if (component) {
            component->OnSceneLoaded(*this);
         }
      }
   }
}

std::vector<Object*> SceneWorld::CollectObjects() const {
   std::vector<Object*> objects;
   objects.reserve(
      objectStore_.GetModels().size() + objectStore_.GetSprites().size() +
      objectStore_.GetUITexts().size() + genericObjects_.size() + skyboxes_.size());
   for (const auto& model : objectStore_.GetModels()) {
      if (model) { objects.push_back(model.get()); }
   }
   for (const auto& sprite : objectStore_.GetSprites()) {
      if (sprite) { objects.push_back(sprite.get()); }
   }
   for (const auto& text : objectStore_.GetUITexts()) {
      if (text) { objects.push_back(text.get()); }
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
   looseObjectsById_[id] = object;
   looseObjectIds_[object] = id;
}

} // namespace GameEngine
