#pragma once

#include <cstdint>
#include "GameEngine/Scene/SceneObjectStore.h"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace GameEngine {

class Object;
class ParticleSystem;
class VirtualCamera;
class CinemachineBrain;

/// @brief JSONシーンが所有するオブジェクトとカメラを管理する
class SceneWorld final {
public:
   /// @brief 空のシーンワールドを作成する
   /// @param cameraBrain カメラの登録先。カメラを読み込む前に明示的に設定する。
   explicit SceneWorld(CinemachineBrain* cameraBrain = nullptr);

   /// @brief シーン初期化時に、カメラの登録先を明示的に関連付ける。
   void SetCameraBrain(CinemachineBrain* brain) { cameraBrain_ = brain; }

   /// @brief World内のObjectとパーティクルを一度ずつ更新してからカメラを評価する。
   void Update(float deltaTime);

   /// @brief 描画終了後のフレーム境界で、遅延削除した実体を解放する。
   void FinishFrame();

   /// @brief デストラクタ
   ~SceneWorld();

   /// @brief 保存シーンの形式バージョン。
   static constexpr std::int32_t kFormatVersion = 7;

   /// @brief 所有物の変更前に保存形式・型・IDを検証する。
   static bool IsValidSnapshot(const nlohmann::json& sceneData);

   /// @brief 全所有物とカメラ・描画設定を完全スナップショットとして保存する。
   nlohmann::json SerializeToJson() const;

   /// @brief 共通の生成・削除・復元ストアを取得する。
   SceneObjectStore& GetObjectStore() { return objectStore_; }

   /// @brief 共通ストアを読み取り専用で取得する。
   const SceneObjectStore& GetObjectStore() const { return objectStore_; }

   /// @brief JSONからシーン全体を生成する
   /// @param sceneData シーン定義JSON
   /// @return 必須データを生成できた場合はtrue。構築失敗時は部分生成物を解除する。
   bool LoadFromJson(const nlohmann::json& sceneData);

   /// @brief 所有するシーン要素をすべて破棄する
   void Clear();

   /// @brief 安定IDからオブジェクトを検索する
   /// @param objectId JSONに保存されたオブジェクトID
   /// @return 対応するオブジェクト。存在しない場合はnullptr
   Object* FindObjectById(const std::string& objectId);

   /// @brief 読み取り専用の対象を検索する。
   const Object* FindObjectById(const std::string& objectId) const;

   /// @brief 安定IDからパーティクルシステムを検索する
   /// @param objectId JSONに保存されたオブジェクトID
   /// @return 対応するパーティクルシステム。存在しない場合はnullptr
   ParticleSystem* FindParticleSystemById(const std::string& objectId);

   /// @brief 読み取り専用の対象を検索する。
   const ParticleSystem* FindParticleSystemById(const std::string& objectId) const;

   /// @brief 安定IDから仮想カメラを検索する
   /// @param cameraId カメラの安定ID
   /// @return 対応する仮想カメラ。存在しない場合はnullptr
   VirtualCamera* FindVirtualCamera(const std::string& cameraId);

   /// @brief 読み取り専用の対象を検索する。
   const VirtualCamera* FindVirtualCamera(const std::string& cameraId) const;

   /// @brief 一意な安定IDを持つ仮想カメラを作成しBrainへ登録する
   /// @param name 表示名。空ならVirtual Cameraを使用する
   /// @return 作成したカメラ。Brainがない場合はnullptr
   VirtualCamera* CreateVirtualCamera(const std::string& name);

   /// @brief このワールドのカメラをBrainから解除して削除する
   /// @return 管理対象のカメラを削除した場合はtrue
   bool RemoveVirtualCamera(VirtualCamera* camera);

   /// @brief オブジェクトに割り当てられた安定IDを取得する
   /// @param object 対象オブジェクト
   /// @return 安定ID。管理対象外の場合は空文字列
   std::string GetObjectId(const Object* object) const;

   /// @brief 全要素を復元した後、指定Entity群の親子関係とComponent参照を再解決する
   /// @param objects 共通Storeに復元されたEntity
   /// @param initializeRuntime falseなら参照だけを更新する
   void ResolveReferences(const std::vector<Object*>& objects, bool initializeRuntime = true);

private:
   CinemachineBrain* GetCameraBrain() const;
   bool ApplySnapshot(const nlohmann::json& sceneData);
   void RestoreCameras(const nlohmann::json& sceneData);

   CinemachineBrain* cameraBrain_ = nullptr;
   SceneObjectStore objectStore_;
   std::vector<std::unique_ptr<VirtualCamera>> virtualCameras_;
   std::unordered_map<std::string, VirtualCamera*> virtualCamerasById_;
};

} // namespace GameEngine
