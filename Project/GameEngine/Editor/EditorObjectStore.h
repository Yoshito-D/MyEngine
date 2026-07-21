#pragma once

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
class Sprite;
class UIText;

class EditorObjectStore {
public:
   /// @brief 所有するエディター生成オブジェクトを破棄する
   ~EditorObjectStore();

   /// @brief 描画機能を持たないGenericオブジェクトを作成する
   /// @param initialTransform 初期トランスフォーム。省略時は既定値
   /// @param requestedId 復元時に使用するオブジェクトID
   /// @return 作成したGenericオブジェクト
   Object* CreateGenericObject(const Transform* initialTransform = nullptr, const std::string& requestedId = {});
   Object* CreateModel(const std::string& assetId, const Transform* initialTransform = nullptr, const std::string& requestedId = {});
   Object* CreateSprite(const std::string& textureAssetId, const Transform* initialTransform = nullptr, const std::string& requestedId = {});
   /// @brief エディター管理のUIテキストを作成する
   /// @param initialTransform 初期スクリーン座標。省略時は既定値を使用する
   /// @param requestedId 復元時に使用するオブジェクトID
   /// @return 作成したUIテキスト。作成できなかった場合はnullptr
   Object* CreateUIText(const Transform* initialTransform = nullptr, const std::string& requestedId = {});
   ParticleSystem* CreateParticleSystem(const std::string& assetId, const std::string& requestedId = {}, const Transform* initialTransform = nullptr);
   Object* RestoreObject(const nlohmann::json& objectData);
   ParticleSystem* RestoreParticleSystem(const nlohmann::json& objectData);
   nlohmann::json SerializeObjectState(const Object* object, const std::string& id = {}) const;
   bool ApplyObjectState(Object* object, const nlohmann::json& objectData) const;
   nlohmann::json SerializeParticleSystemState(const ParticleSystem* particleSystem, const std::string& id = {}, const std::string& assetId = {}) const;
   bool ApplyParticleSystemState(ParticleSystem* particleSystem, const nlohmann::json& objectData) const;

   bool DeleteObject(const std::string& objectId);
   bool DeleteParticleSystem(const std::string& objectId);
   void FlushDeferredDeletes();
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

   /// @brief エディターが所有するGenericオブジェクト一覧を取得する
   /// @return Genericオブジェクト一覧
   const std::vector<std::unique_ptr<Object>>& GetGenericObjects() const { return genericObjects_; }
   const std::vector<std::unique_ptr<Model>>& GetModels() const { return models_; }
   const std::vector<std::unique_ptr<Sprite>>& GetSprites() const { return sprites_; }
   /// @brief エディターが所有するUIテキスト一覧を取得する
   /// @return UIテキスト一覧
   const std::vector<std::unique_ptr<UIText>>& GetUITexts() const { return uiTexts_; }
   const std::vector<std::unique_ptr<ParticleSystem>>& GetParticleSystems() const { return particleSystems_; }

private:
   std::string AllocateId(const std::string& requestedId);
   std::string BuildUniqueObjectName(const std::string& baseName) const;
   void RegisterObject(const std::string& id, Object* object);
   void RegisterParticleSystem(const std::string& id, ParticleSystem* particleSystem, const std::string& assetId);
   void UnregisterObject(Object* object);
   void UnregisterParticleSystem(ParticleSystem* particleSystem);
   void UnregisterOwnedRuntimeSystems(Object* object);
   void BumpCounterFromId(const std::string& id);
   void DeserializeSpriteData(Sprite* sprite, const nlohmann::json& data) const;
   bool EnsureTextureLoaded(const std::string& textureAssetId) const;

   static nlohmann::json SerializeSpriteData(const Sprite* sprite);

   std::vector<std::unique_ptr<Object>> genericObjects_;
   std::vector<std::unique_ptr<Model>> models_;
   std::vector<std::unique_ptr<Sprite>> sprites_;
   std::vector<std::unique_ptr<UIText>> uiTexts_;
   std::vector<std::unique_ptr<ParticleSystem>> particleSystems_;
   std::vector<std::unique_ptr<Object>> deferredDeleteGenericObjects_;
   std::vector<std::unique_ptr<Model>> deferredDeleteModels_;
   std::vector<std::unique_ptr<Sprite>> deferredDeleteSprites_;
   std::vector<std::unique_ptr<UIText>> deferredDeleteUITexts_;
   std::vector<std::unique_ptr<ParticleSystem>> deferredDeleteParticleSystems_;
   std::unordered_map<std::string, Object*> idToObject_;
   std::unordered_map<std::string, ParticleSystem*> idToParticleSystem_;
   std::unordered_map<const Object*, std::string> objectToId_;
   std::unordered_map<const ParticleSystem*, std::string> particleSystemToId_;
   std::unordered_map<const ParticleSystem*, std::string> particleSystemAssetIds_;
   uint64_t nextObjectIndex_ = 1;
};

} // namespace GameEngine
