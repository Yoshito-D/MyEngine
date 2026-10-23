#pragma once

#include "GameEngine/Math/MathUtils.h"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace GameEngine {

class SceneWorld;
class Model;
class Object;
class ParticleSystem;
class Skybox;
class Sprite;
class UIText;

/// @brief 保存シーンから生成する各Object型を所有し、安定IDと遅延削除を管理する。
class SceneObjectStore {
public:
   /// @brief 指定Worldに所属するObjectとパーティクルの所有ストアを作成する。
   explicit SceneObjectStore(SceneWorld& world);

   /// @brief 所有するシーンオブジェクトを破棄する。
   ~SceneObjectStore();

   /// @brief 描画機能を持たないGenericオブジェクトを作成する
   /// @param initialTransform 初期トランスフォーム。省略時は既定値
   /// @param requestedId 復元時に使用するオブジェクトID
   /// @return 作成したGenericオブジェクト
   Object* CreateGenericObject(const Transform* initialTransform = nullptr, const std::string& requestedId = {});

   /// @brief モデルアセットを参照するModelを作成する
   /// @param assetId resources相対のモデルID
   /// @param initialTransform 初期トランスフォーム
   /// @param requestedId 復元時に再利用するID
   /// @return 作成したModel。ロードできない場合はnullptr
   Object* CreateModel(
      const std::string& assetId, const Transform* initialTransform = nullptr, const std::string& requestedId = {});

   /// @brief テクスチャを表示するSpriteを作成する
   /// @param textureAssetId resources相対のテクスチャID
   /// @param initialTransform 初期トランスフォーム
   /// @param requestedId 復元時に再利用するID
   /// @return 作成したSprite。ロードできない場合はnullptr
   Object* CreateSprite(const std::string& textureAssetId, const Transform* initialTransform = nullptr,
      const std::string& requestedId = {});

   /// @brief UIテキストを作成する
   /// @param initialTransform 初期スクリーン座標。省略時は既定値を使用する
   /// @param requestedId 復元時に使用するオブジェクトID
   /// @return 作成したUIテキスト。作成できなかった場合はnullptr
   Object* CreateUIText(const Transform* initialTransform = nullptr, const std::string& requestedId = {});

   /// @brief スカイボックスを作成する
   /// @param requestedId 復元時に使用するオブジェクトID
   /// @return 作成したスカイボックス。作成できなかった場合はnullptr
   Object* CreateSkybox(const std::string& requestedId = {});

   /// @brief JSONアセットを読み込んだParticleSystemを作成して再生する
   /// @param assetId resources相対のパーティクルID
   /// @param requestedId 復元時に再利用するID
   /// @param initialTransform 初期エミッタートランスフォーム
   /// @return 作成したParticleSystem
   ParticleSystem* CreateParticleSystem(
      const std::string& assetId, const std::string& requestedId = {}, const Transform* initialTransform = nullptr);

   /// @brief 種別付きJSONから対応するObject派生型を作成して状態を復元する
   /// @param objectData SerializeObjectState形式のJSON
   /// @return 復元したObject。ParticleSystemまたは失敗時はnullptr
   Object* RestoreObject(const nlohmann::json& objectData);

   /// @brief 種別付きJSONからParticleSystemを作成して状態を復元する
   /// @param objectData SerializeParticleSystemState形式のJSON
   /// @return 復元したParticleSystem。失敗時はnullptr
   ParticleSystem* RestoreParticleSystem(const nlohmann::json& objectData);

   /// @brief 任意のObjectを具象型情報付きJSONへ変換する
   /// @param object シリアライズ対象
   /// @param id 保存する安定ID
   /// @return 復元可能なオブジェクトJSON
   nlohmann::json SerializeObjectState(const Object* object, const std::string& id = {}) const;

   /// @brief 既存Objectへ同じ具象型の保存状態を適用する
   /// @param object 適用対象
   /// @param objectData SerializeObjectState形式のJSON
   /// @return 型が一致し状態を適用できた場合はtrue
   bool ApplyObjectState(Object* object, const nlohmann::json& objectData) const;

   /// @brief ParticleSystemをアセットIDと固有設定を含むJSONへ変換する
   /// @param particleSystem シリアライズ対象
   /// @param id 保存する安定ID
   /// @param assetId 元になったパーティクルアセットID
   /// @return 復元可能なパーティクルJSON
   nlohmann::json SerializeParticleSystemState(
      const ParticleSystem* particleSystem, const std::string& id = {}, const std::string& assetId = {}) const;

   /// @brief 既存ParticleSystemへ保存状態を適用する
   /// @param particleSystem 適用対象
   /// @param objectData SerializeParticleSystemState形式のJSON
   /// @return 状態を適用できた場合はtrue
   bool ApplyParticleSystemState(ParticleSystem* particleSystem, const nlohmann::json& objectData) const;

   /// @brief IDに対応するObjectをレジストリから外して遅延削除へ移す
   /// @param objectId 削除対象ID
   /// @return 対象が存在して削除予約できた場合はtrue
   bool DeleteObject(const std::string& objectId);

   /// @brief IDに対応するParticleSystemをレジストリから外して遅延削除へ移す
   /// @param objectId 削除対象ID
   /// @return 対象が存在して削除予約できた場合はtrue
   bool DeleteParticleSystem(const std::string& objectId);

   /// @brief 描画が完了した削除待ちの実体を破棄する
   void FlushDeferredDeletes();

   /// @brief Objectが削除待ちで編集対象から外れているかを調べる
   /// @param object 確認するObject。参照先のメモリは読み取らない
   /// @return このストアで削除待ちの場合はtrue
   bool IsPendingDeletion(const Object* object) const { return pendingDeletionObjects_.contains(object); }

   /// @brief 全所有物を登録解除して遅延削除へ移し、ID表を空にする
   void Clear();

   /// @brief Objectがこのストアの所有物か調べる
   /// @param object 確認対象
   /// @return 所有している場合はtrue
   bool Contains(const Object* object) const;

   /// @brief ParticleSystemがこのストアの所有物か調べる
   /// @param particleSystem 確認対象
   /// @return 所有している場合はtrue
   bool Contains(const ParticleSystem* particleSystem) const;

   /// @brief Object種別を問わずIDが使用中か調べる
   /// @param objectId 確認するID
   /// @return 使用中の場合はtrue
   bool ContainsId(const std::string& objectId) const;

   /// @brief Objectに対応する安定IDを取得する
   /// @param object 検索対象
   /// @return 対応ID。未登録の場合は空文字列
   std::string GetId(const Object* object) const;

   /// @brief ParticleSystemに対応する安定IDを取得する
   /// @param particleSystem 検索対象
   /// @return 対応ID。未登録の場合は空文字列
   std::string GetId(const ParticleSystem* particleSystem) const;

   /// @brief 安定IDからObjectを検索する
   /// @param objectId 検索するID
   /// @return 対応Object。未登録の場合はnullptr
   Object* FindById(const std::string& objectId);

   /// @brief 読み取り専用の対象を検索する。
   const Object* FindById(const std::string& objectId) const;

   /// @brief 安定IDからParticleSystemを検索する
   /// @param objectId 検索するID
   /// @return 対応ParticleSystem。未登録の場合はnullptr
   ParticleSystem* FindParticleById(const std::string& objectId);

   /// @brief 読み取り専用の対象を検索する。
   const ParticleSystem* FindParticleById(const std::string& objectId) const;

   /// @brief IDに対応するObjectまたはParticleSystemをシリアライズする
   /// @param objectId 対象ID
   /// @return 復元可能なJSON。未登録の場合は空オブジェクト
   nlohmann::json SerializeObject(const std::string& objectId) const;

   /// @brief 所有する全ObjectとParticleSystemを配列へシリアライズする
   /// @return 種別付きオブジェクトJSON配列
   nlohmann::json SerializeAll() const;

   /// @brief エディターが所有するGenericオブジェクト一覧を取得する
   /// @return Genericオブジェクト一覧
   std::vector<Object*> GetGenericObjects();

   /// @brief 所有権を公開せず、読み取り用の要素一覧を作る。
   std::vector<const Object*> GetGenericObjects() const;

   /// @brief エディタが所有するModel一覧を取得する
   /// @return Model一覧
   std::vector<Model*> GetModels();

   /// @brief 所有権を公開せず、読み取り用の要素一覧を作る。
   std::vector<const Model*> GetModels() const;

   /// @brief エディタが所有するSprite一覧を取得する
   /// @return Sprite一覧
   std::vector<Sprite*> GetSprites();

   /// @brief 所有権を公開せず、読み取り用の要素一覧を作る。
   std::vector<const Sprite*> GetSprites() const;

   /// @brief エディターが所有するUIテキスト一覧を取得する
   /// @return UIテキスト一覧
   std::vector<UIText*> GetUITexts();

   /// @brief 所有権を公開せず、読み取り用の要素一覧を作る。
   std::vector<const UIText*> GetUITexts() const;

   /// @brief エディタが所有するスカイボックス一覧を取得する
   /// @return スカイボックス一覧
   std::vector<Skybox*> GetSkyboxes();

   /// @brief 所有権を公開せず、読み取り用の要素一覧を作る。
   std::vector<const Skybox*> GetSkyboxes() const;

   /// @brief エディタが所有するParticleSystem一覧を取得する
   /// @return ParticleSystem一覧
   std::vector<ParticleSystem*> GetParticleSystems();

   /// @brief 所有権を公開せず、読み取り用の要素一覧を作る。
   std::vector<const ParticleSystem*> GetParticleSystems() const;

   /// @brief 所有する全Objectを列挙する。
   std::vector<Object*> GetObjects();

   /// @brief 所有する全Objectを読み取り専用で列挙する。
   std::vector<const Object*> GetObjects() const;

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

   static nlohmann::json SerializeSpriteData(const Sprite* sprite);

   SceneWorld& world_;                            ///< 所属シーンのオブジェクトと実行時状態を参照するワールド。
   std::vector<std::unique_ptr<Object>> objects_; ///< シーン内で生成したObjectの所有一覧。
   std::vector<std::unique_ptr<ParticleSystem>> particleSystems_; ///< シーン内で生成したParticleSystemの所有一覧。
   /// @brief GPU参照が完了するフレーム境界まで寿命を保持する削除待ちObject。
   std::vector<std::unique_ptr<Object>> deferredDeleteObjects_;
   /// @brief フレーム境界まで寿命を保持する削除待ちParticleSystem。
   std::vector<std::unique_ptr<ParticleSystem>> deferredDeleteParticleSystems_;
   std::unordered_set<const Object*> pendingDeletionObjects_; ///< 更新や検索の対象から外した削除待ちObjectの集合。
   std::unordered_map<std::string, Object*> idToObject_;      ///< 安定Entity IDからObjectを引く非所有索引。
   /// @brief 安定IDからParticleSystemを引く非所有索引。
   std::unordered_map<std::string, ParticleSystem*> idToParticleSystem_;
   std::unordered_map<const Object*, std::string> objectToId_; ///< Objectのアドレスから安定Entity IDを引く逆索引。
   /// @brief ParticleSystemのアドレスから安定IDを引く逆索引。
   std::unordered_map<const ParticleSystem*, std::string> particleSystemToId_;
   /// @brief 各ParticleSystemの生成元となったアセットID。
   std::unordered_map<const ParticleSystem*, std::string> particleSystemAssetIds_;
   uint64_t nextObjectIndex_ = 1; ///< 一時IDや自動生成Entity IDに使う次の通し番号。
};

} // namespace GameEngine
