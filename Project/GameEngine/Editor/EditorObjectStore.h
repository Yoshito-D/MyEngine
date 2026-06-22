#pragma once

#ifdef USE_IMGUI

#include "MathUtils.h"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace GameEngine {

class Model;
class Object;
class ParticleSystem;

class EditorObjectStore {
public:
   Object* CreateModel(const std::string& assetId, const Transform* initialTransform = nullptr, const std::string& requestedId = {});
   ParticleSystem* CreateParticleSystem(const std::string& assetId, const std::string& requestedId = {});
   Object* RestoreObject(const nlohmann::json& objectData);

   bool DeleteObject(const std::string& objectId);
   bool DeleteParticleSystem(const std::string& objectId);
   void Clear();

   bool Contains(const Object* object) const;
   bool Contains(const ParticleSystem* particleSystem) const;
   bool ContainsId(const std::string& objectId) const;
   std::string GetId(const Object* object) const;
   std::string GetId(const ParticleSystem* particleSystem) const;
   Object* FindById(const std::string& objectId) const;
   ParticleSystem* FindParticleById(const std::string& objectId) const;

   nlohmann::json SerializeObject(const std::string& objectId) const;
   nlohmann::json SerializeAll() const;

   const std::vector<std::unique_ptr<Model>>& GetModels() const { return models_; }
   const std::vector<std::unique_ptr<ParticleSystem>>& GetParticleSystems() const { return particleSystems_; }

private:
   std::string AllocateId(const std::string& requestedId);
   std::string BuildUniqueObjectName(const std::string& baseName) const;
   void RegisterObject(const std::string& id, Model* model);
   void RegisterParticleSystem(const std::string& id, ParticleSystem* particleSystem, const std::string& assetId);
   void UnregisterObject(Model* model);
   void UnregisterParticleSystem(ParticleSystem* particleSystem);
   void BumpCounterFromId(const std::string& id);

   std::vector<std::unique_ptr<Model>> models_;
   std::vector<std::unique_ptr<ParticleSystem>> particleSystems_;
   std::unordered_map<std::string, Model*> idToModel_;
   std::unordered_map<std::string, ParticleSystem*> idToParticleSystem_;
   std::unordered_map<const Object*, std::string> objectToId_;
   std::unordered_map<const ParticleSystem*, std::string> particleSystemToId_;
   std::unordered_map<const ParticleSystem*, std::string> particleSystemAssetIds_;
   uint64_t nextObjectIndex_ = 1;
};

} // namespace GameEngine

#endif
