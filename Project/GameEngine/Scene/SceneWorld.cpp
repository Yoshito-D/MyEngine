#include "GameEngine/pch.h"
#include "GameEngine/Scene/SceneWorld.h"

#include "GameEngine/Effects/Particles/ParticleSystem.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Object/Component/Base/ComponentRegistry.h"
#include "GameEngine/Utility/Logger.h"
#include "GameEngine/Scene/Camera/Core/CinemachineBrain.h"
#include "GameEngine/Scene/Camera/Core/VirtualCamera.h"
#include <algorithm>
#include <objbase.h>
#include <unordered_set>

namespace GameEngine {

namespace {
bool HasOptionalJsonType(const nlohmann::json& data, const char* key, nlohmann::json::value_t type) {
   const auto it = data.find(key);
   return it == data.end() || it->type() == type;
}

bool IsValidComponentState(const nlohmann::json& data, const char* typeKey) {
   return data.is_object() && HasOptionalJsonType(data, typeKey, nlohmann::json::value_t::string) &&
          HasOptionalJsonType(data, "enabled", nlohmann::json::value_t::boolean) &&
          HasOptionalJsonType(data, "data", nlohmann::json::value_t::object) &&
          HasOptionalJsonType(data, "parameterAssetId", nlohmann::json::value_t::string);
}

bool IsValidObjectState(const nlohmann::json& data) {
   if (!data.is_object()) {
      return false;
   }

   for (const auto* key : {"id", "parentId", "objectType", "assetId", "name"}) {
      if (!HasOptionalJsonType(data, key, nlohmann::json::value_t::string)) {
         return false;
      }
   }

   for (const auto* key : {"data", "sprite"}) {
      if (!HasOptionalJsonType(data, key, nlohmann::json::value_t::object)) {
         return false;
      }
   }

   if (!HasOptionalJsonType(data, "components", nlohmann::json::value_t::array)) {
      return false;
   }

   if (data.contains("components")) {
      for (const auto& component : data.at("components")) {
         if (!IsValidComponentState(component, "typeName")) {
            return false;
         }
      }
   }

   return true;
}
} // namespace

SceneWorld::SceneWorld(CinemachineBrain* cameraBrain) : cameraBrain_(cameraBrain), objectStore_(*this) {
}

SceneWorld::~SceneWorld() {
   Clear();
}

CinemachineBrain* SceneWorld::GetCameraBrain() const {
   return cameraBrain_;
}

bool SceneWorld::IsValidSnapshot(const nlohmann::json& data) {
   if (!data.is_object() || !data.contains("version") || !data.at("version").is_number_integer() ||
       data.at("version") != kFormatVersion || !data.contains("objects") || !data.at("objects").is_array() ||
       !data.contains("cameras") || !data.at("cameras").is_object() ||
       !HasOptionalJsonType(data, "renderSettings", nlohmann::json::value_t::object) ||
       !HasOptionalJsonType(data, "hierarchyOrder", nlohmann::json::value_t::array)) {
      return false;
   }

   std::unordered_set<std::string> ids;

   for (const auto& object : data.at("objects")) {
      if (!IsValidObjectState(object) || !object.contains("id") || !object.contains("objectType") ||
          object.at("id").get<std::string>().empty() || !ids.insert(object.at("id").get<std::string>()).second) {
         return false;
      }
   }

   for (const auto& object : data.at("objects")) {
      const auto kind = object.at("objectType").get<std::string>();
      if (kind == "ParticleSystem") {
         if (object.contains("components") && !object.at("components").empty()) {
            return false;
         }
         continue;
      }
      ObjectType type;
      if (kind == "Generic") {
         type = ObjectType::Generic;
      } else if (kind == "Model") {
         type = ObjectType::Model;
      } else if (kind == "Sprite") {
         type = ObjectType::Sprite;
      } else if (kind == "UIText") {
         type = ObjectType::UIText;
      } else if (kind == "Skybox") {
         type = ObjectType::Skybox;
      } else {
         return false;
      }
      if (!object.contains("components")) {
         return false;
      }
      std::unordered_set<std::string> componentTypes;
      for (const auto& component : object.at("components")) {
         const auto name = component.value("typeName", std::string{});
         if (!componentTypes.insert(name).second || !ComponentRegistry::GetInstance().Supports(name, type)) {
            return false;
         }
      }
   }

   const auto& cameras = data.at("cameras");

   if (!cameras.contains("virtualCameras") || !cameras.at("virtualCameras").is_array() ||
       !HasOptionalJsonType(cameras, "brain", nlohmann::json::value_t::object)) {
      return false;
   }

   ids.clear();

   for (const auto& camera : cameras.at("virtualCameras")) {
      if (!camera.is_object() || !camera.contains("id") || !camera.at("id").is_string() ||
          camera.at("id").get<std::string>().empty() || !ids.insert(camera.at("id").get<std::string>()).second ||
          !HasOptionalJsonType(camera, "name", nlohmann::json::value_t::string) ||
          !HasOptionalJsonType(camera, "followTargetId", nlohmann::json::value_t::string) ||
          !HasOptionalJsonType(camera, "lookAtTargetId", nlohmann::json::value_t::string) ||
          !HasOptionalJsonType(camera, "state", nlohmann::json::value_t::object) ||
          !HasOptionalJsonType(camera, "active", nlohmann::json::value_t::boolean) || !camera.contains("components") ||
          !camera.at("components").is_array() ||
          (camera.contains("priority") && !camera.at("priority").is_number_integer())) {
         return false;
      }

      std::unordered_set<std::string> componentTypes;
      const auto registered = VirtualCamera::GetRegisteredComponentNames();
      for (const auto& component : camera.at("components")) {
         if (!IsValidComponentState(component, "componentName")) {
            return false;
         }
         const auto name = component.value("componentName", std::string{});
         if (!componentTypes.insert(name).second ||
             std::find(registered.begin(), registered.end(), name) == registered.end()) {
            return false;
         }
      }
   }

   if (data.contains("hierarchyOrder")) {
      for (const auto& id : data.at("hierarchyOrder")) {
         if (!id.is_string()) {
            return false;
         }
      }
   }

   return true;
}

nlohmann::json SceneWorld::SerializeToJson() const {
   nlohmann::json cameras = nlohmann::json::array();

   for (const auto& camera : virtualCameras_) {
      cameras.push_back(camera->Serialize());
   }

   auto* brain = GetCameraBrain();
   return {{"version", kFormatVersion}, {"objects", objectStore_.SerializeAll()},
      {"cameras", {{"brain", {{"defaultBlendTime", brain ? brain->GetDefaultBlendTime() : 0.0f}}},
                     {"virtualCameras", std::move(cameras)}}},
      {"renderSettings", EngineContext::SerializePostProcessSceneState()}};
}

bool SceneWorld::LoadFromJson(const nlohmann::json& sceneData) {
   if (!IsValidSnapshot(sceneData)) {
      Logger::Error("SceneWorld load failed: invalid snapshot format or duplicate id");
      return false;
   }
   try {
      if (ApplySnapshot(sceneData)) {
         return true;
      }
   } catch (const std::exception& error) {
      Logger::Error("SceneWorld load failed: " + std::string(error.what()));
   }
   // 復旧元はManagerが所有する。ここでは失敗した構築物だけを解除する。
   Clear();
   return false;
}

bool SceneWorld::ApplySnapshot(const nlohmann::json& sceneData) {
   if (!EngineContext::ApplyPostProcessSceneState(sceneData.value("renderSettings", nlohmann::json::object()))) {
      return false;
   }

   // JSONから除かれたIDも含め、全Entityを同じStoreから置き換える。
   Clear();

   for (const auto& data : sceneData.at("objects")) {
      const bool particle = data.at("objectType") == "ParticleSystem";

      if (particle ? objectStore_.RestoreParticleSystem(data) == nullptr
                   : objectStore_.RestoreObject(data) == nullptr) {
         Logger::Error("SceneWorld could not restore object: " + data.at("id").get<std::string>());
         return false;
      }
   }

   RestoreCameras(sceneData);
   ResolveReferences(objectStore_.GetObjects());
   return true;
}

void SceneWorld::SynchronizeParameters() {
   for (auto* object : objectStore_.GetObjects()) {
      object->SynchronizeParameters();
   }
   for (const auto& camera : virtualCameras_) {
      for (const auto* component : camera->GetComponents()) {
         const_cast<ICinemachineComponent*>(component)->SynchronizeParameters();
      }
   }
}

void SceneWorld::Clear() {
   ParticleSystem::ClearWorldRuntime(this);
   if (auto* brain = GetCameraBrain()) {
      for (const auto& camera : virtualCameras_) {
         brain->UnregisterVirtualCamera(camera.get());
      }

      brain->SetCameraMotionPaused(false);
   }

   virtualCamerasById_.clear();
   virtualCameras_.clear();
   objectStore_.Clear();
   objectStore_.FlushDeferredDeletes();
}

void SceneWorld::Update(float deltaTime) {
   const auto objects = objectStore_.GetObjects();
   for (auto* object : objects) {
      if (objectStore_.Contains(object) && !objectStore_.IsPendingDeletion(object)) {
         object->UpdateComponents(deltaTime);
      }
   }
   const auto particles = ParticleSystem::GetRegisteredParticleSystems();
   for (auto* particle : particles) {
      if (particle && particle->GetWorld() == this) {
         particle->Update(deltaTime);
      }
   }
   ParticleSystem::ProcessPendingSubEmitters(this);
   if (cameraBrain_) {
      cameraBrain_->Update(deltaTime);
   }
}

void SceneWorld::FinishFrame() {
   objectStore_.FlushDeferredDeletes();
}

Object* SceneWorld::FindObjectById(const std::string& objectId) {
   return objectStore_.FindById(objectId);
}

const Object* SceneWorld::FindObjectById(const std::string& objectId) const {
   return objectStore_.FindById(objectId);
}

ParticleSystem* SceneWorld::FindParticleSystemById(const std::string& objectId) {
   return objectStore_.FindParticleById(objectId);
}

const ParticleSystem* SceneWorld::FindParticleSystemById(const std::string& objectId) const {
   return objectStore_.FindParticleById(objectId);
}

VirtualCamera* SceneWorld::CreateVirtualCamera(const std::string& name) {
   auto* brain = GetCameraBrain();

   if (!brain) {
      return nullptr;
   }

   auto camera = std::make_unique<VirtualCamera>();
   camera->Initialize();
   camera->SetName(name.empty() ? "Virtual Camera" : name);

   // 表示名を変更してもコンポーネントからの参照は変わらない。
   GUID guid{};

   if (FAILED(CoCreateGuid(&guid))) {
      return nullptr;
   }

   wchar_t buffer[40]{};
   StringFromGUID2(guid, buffer, 40);

   // GUID表記はASCIIのみなので、文字幅の変換を明示する。
   std::string id;

   for (const wchar_t* character = buffer; *character; ++character) {
      id.push_back(static_cast<char>(*character));
   }

   camera->SetId(id);
   camera->SetWorld(this);
   auto* result = camera.get();
   virtualCameras_.push_back(std::move(camera));
   virtualCamerasById_[id] = result;
   brain->RegisterVirtualCamera(result);
   return result;
}

bool SceneWorld::RemoveVirtualCamera(VirtualCamera* camera) {
   const auto it = std::find_if(
      virtualCameras_.begin(), virtualCameras_.end(), [camera](const auto& entry) { return entry.get() == camera; });

   if (it == virtualCameras_.end()) {
      return false;
   }

   if (auto* brain = GetCameraBrain()) {
      brain->UnregisterVirtualCamera(camera);
   }

   virtualCamerasById_.erase(camera->GetId());
   virtualCameras_.erase(it);
   return true;
}

void SceneWorld::RestoreCameras(const nlohmann::json& sceneData) {
   if (!sceneData.contains("cameras") || !sceneData.at("cameras").is_object()) return;

   // カメラの登録先がない場合も失敗として通知し、不完全なシーンを成功扱いにしない。
   auto* brain = GetCameraBrain();

   if (!brain) {
      throw std::runtime_error("Scene requires an output camera brain");
   }

   const auto& camerasData = sceneData.at("cameras");

   if (camerasData.contains("brain") && camerasData.at("brain").is_object()) {
      const auto& brainData = camerasData.at("brain");

      if (brainData.contains("defaultBlendTime") && brainData.at("defaultBlendTime").is_number()) {
         brain->SetDefaultBlendTime(brainData.at("defaultBlendTime").get<float>());
      }
   }

   if (!camerasData.contains("virtualCameras") || !camerasData.at("virtualCameras").is_array()) return;

   for (const auto& cameraData : camerasData.at("virtualCameras")) {
      if (!cameraData.is_object()) {
         continue;
      }

      auto camera = std::make_unique<VirtualCamera>();
      VirtualCamera* rawCamera = camera.get();
      rawCamera->SetWorld(this);
      rawCamera->Initialize();
      if (!rawCamera->Deserialize(cameraData)) {
         throw std::runtime_error("Camera component restoration failed");
      }
      const std::string id = rawCamera->GetId();

      if (id.empty() || virtualCamerasById_.contains(id)) {
         throw std::runtime_error("Camera id is empty or duplicated");
      }

      // BrainとID索引は非所有ポインターを保持する。最後にunique_ptrをコンテナへ移し、
      // 以降の参照先アドレスがワールド破棄まで安定するようにする。
      brain->RegisterVirtualCamera(rawCamera);
      virtualCamerasById_[id] = rawCamera;
      virtualCameras_.push_back(std::move(camera));
   }
}

VirtualCamera* SceneWorld::FindVirtualCamera(const std::string& cameraId) {
   auto it = virtualCamerasById_.find(cameraId);
   return it == virtualCamerasById_.end() ? nullptr : it->second;
}

const VirtualCamera* SceneWorld::FindVirtualCamera(const std::string& cameraId) const {
   auto it = virtualCamerasById_.find(cameraId);
   return it == virtualCamerasById_.end() ? nullptr : it->second;
}

std::string SceneWorld::GetObjectId(const Object* object) const {
   return objectStore_.GetId(object);
}

void SceneWorld::ResolveReferences(const std::vector<Object*>& objects, bool initializeRuntime) {
   // 参照先をすべて生成してから解決し、JSON内の順序への依存をなくす。
   for (Object* object : objects) {
      if (object) {
         object->ResolveComponentReferences(*this, initializeRuntime);
      }
   }
}

} // namespace GameEngine
