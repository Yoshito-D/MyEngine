#pragma once

#include "Editor/EditorObjectStore.h"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace GameEngine {

class Object;
class ParticleSystem;
class Skybox;
class VirtualCamera;
class CinemachineBrain;

/// @brief JSONシーンが所有するオブジェクトとカメラを管理する
class SceneWorld final {
public:
   /// @brief 空のシーンワールドを作成する
   /// @param cameraBrain カメラ登録先。nullptrならEngineContextの現在のBrainを使用する
   explicit SceneWorld(CinemachineBrain* cameraBrain = nullptr);

   /// @brief デストラクタ
   ~SceneWorld();

   /// @brief JSONからシーン全体を生成する
   /// @param sceneData シーン定義JSON
   /// @return 必須データを生成できた場合はtrue
   bool LoadFromJson(const nlohmann::json& sceneData);

   /// @brief 所有するシーン要素をすべて破棄する
   void Clear();

   /// @brief Model等とは別に管理される汎用オブジェクトを更新する
   void Update(float);

   /// @brief 安定IDからオブジェクトを検索する
   /// @param objectId JSONに保存されたオブジェクトID
   /// @return 対応するオブジェクト。存在しない場合はnullptr
   Object* FindObjectById(const std::string& objectId) const;

   /// @brief 安定IDからパーティクルシステムを検索する
   /// @param objectId JSONに保存されたオブジェクトID
   /// @return 対応するパーティクルシステム。存在しない場合はnullptr
   ParticleSystem* FindParticleSystemById(const std::string& objectId) const;

   /// @brief 表示名からオブジェクトを検索する
   /// @param objectName ObjectNameComponentの名前
   /// @return 最初に一致したオブジェクト。存在しない場合はnullptr
   Object* FindObjectByName(const std::string& objectName) const;

   /// @brief IDまたは名前から仮想カメラを検索する
   /// @param cameraIdOrName カメラIDまたは表示名
   /// @return 対応する仮想カメラ。存在しない場合はnullptr
   VirtualCamera* FindVirtualCamera(const std::string& cameraIdOrName) const;

   /// @brief 一意な安定IDを持つ仮想カメラを作成しBrainへ登録する
   /// @param name 表示名。空ならVirtual Cameraを使用する
   /// @return 作成したカメラ。Brainがない場合はnullptr
   VirtualCamera* CreateVirtualCamera(const std::string& name);
   /// @brief このワールドのカメラをBrainから解除して削除する
   /// @return 管理対象のカメラを削除した場合はtrue
   bool RemoveVirtualCamera(VirtualCamera* camera);
   /// @brief 保存されたカメラ一覧へ構成を置き換える（DebugCameraは維持する）
   /// @param camerasData シーンJSONのcamerasオブジェクト
   void ApplyCameraConfiguration(const nlohmann::json& camerasData);

   /// @brief オブジェクトに割り当てられた安定IDを取得する
   /// @param object 対象オブジェクト
   /// @return 安定ID。管理対象外の場合は空文字列
   std::string GetObjectId(const Object* object) const;

   /// @brief 現在ロードされているシーンワールドを取得する
   /// @return 現在のシーンワールド。未ロードの場合はnullptr
   static SceneWorld* GetCurrent() { return sCurrent_; }

   /// @brief Model等の描画オブジェクトとは別に所有するGenericオブジェクト一覧を取得する
   /// @return Genericオブジェクト一覧
   const std::vector<std::unique_ptr<Object>>& GetGenericObjects() const { return genericObjects_; }

   /// @brief 全要素を復元した後、指定Entity群の親子関係とComponent参照を再解決する
   /// @param objects シーン所有分とエディター所有分を含む復元済みEntity
   /// @param initializeRuntime falseなら参照だけを更新する
   void ResolveReferences(const std::vector<Object*>& objects, bool initializeRuntime = true);

private:
   CinemachineBrain* GetCameraBrain() const;
   CinemachineBrain* cameraBrain_ = nullptr;
   bool RestoreObjectEntry(const nlohmann::json& objectData, const std::string& legacySceneKey = {});
   void RestoreLegacyEntries(const nlohmann::json& sceneData);
   void RestoreLegacyLights(const nlohmann::json& sceneData);
   void RestoreCameras(const nlohmann::json& sceneData);
   void ResolveReferences();
   std::vector<Object*> CollectObjects() const;
   void RegisterLooseObject(const std::string& id, Object* object);

   EditorObjectStore objectStore_;
   std::vector<std::unique_ptr<Object>> genericObjects_;
   std::vector<std::unique_ptr<Skybox>> skyboxes_;
   std::vector<std::unique_ptr<VirtualCamera>> virtualCameras_;
   std::unordered_map<std::string, Object*> looseObjectsById_;
   std::unordered_map<const Object*, std::string> looseObjectIds_;
   std::unordered_map<std::string, VirtualCamera*> virtualCamerasById_;

   static inline SceneWorld* sCurrent_ = nullptr;
};

} // namespace GameEngine
