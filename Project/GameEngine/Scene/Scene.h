#pragma once

#include "GameEngine/Scene/SceneDefinition.h"
#include "GameEngine/Scene/SceneWorld.h"

namespace GameEngine {

class AssetManager;
class CameraManager;

/// @brief JSON定義から構築する、共通の具象シーン。
class Scene final {
public:
   /// @brief シーン名と実ファイルパスを指定し、未ロードのシーンを作成する。
   /// @param definition シーン名と実ファイルパス。
   /// @param cameras シーンの出力カメラを所有するマネージャー。シーンより長く生存すること。
   Scene(SceneDefinition definition, CameraManager& cameras);

   /// @brief エンジンの各機能を破棄する前にWorldの資源を解放する。
   ~Scene();

   /// @brief スナップショットを検証し、現在のシーンを切り離す前に参照アセットを準備する。
   /// @param data 構築予定のシーンスナップショット。
   /// @param assets 共有設定とテクスチャの取得元。
   /// @return 検証とテクスチャの先読みが完了した場合にtrue。
   static bool PrepareAssets(const nlohmann::json& data, AssetManager& assets);

   /// @brief 出力カメラを作成し、アセットを準備済みのスナップショットを復元する。
   bool Initialize(const nlohmann::json& data);

   /// @brief このシーンに所属する資源だけを解放する。
   void Finalize();

   /// @brief シーン名と読み書き先の実ファイルを取得する。
   const SceneDefinition& GetDefinition() const { return definition_; }

   /// @brief シーン内容を管理するWorldを取得する。
   SceneWorld& GetSceneWorld() { return world_; }

   /// @brief シーン内容を読み取り専用で取得する。
   const SceneWorld& GetSceneWorld() const { return world_; }

private:
   CameraManager& cameras_;     ///< 出力カメラの所有先。
   SceneDefinition definition_; ///< シーン名と読み書き先の実ファイル。
   SceneWorld world_;           ///< シーン内容を管理するWorld。
   bool initialized_ = false;   ///< 初期化が完了し、対応する終了処理が必要かどうか。
};
}
