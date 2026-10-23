#pragma once

#include "GameEngine/Scene/SceneDefinition.h"
#include "GameEngine/Scene/SceneWorld.h"

namespace GameEngine {

/// @brief JSON定義から構築する、共通の具象シーン。
class Scene final {
public:
   /// @brief シーン名と実ファイルパスを指定し、未ロードのシーンを作成する。
   explicit Scene(SceneDefinition definition);

   /// @brief エンジンの各機能を破棄する前にWorldの資源を解放する。
   ~Scene();

   /// @brief 出力カメラを作成し、検証済みの完全スナップショットを復元する。
   bool Initialize(const nlohmann::json& data);

   /// @brief このシーンに所属する資源だけを解放する。
   void Finalize();

   /// @brief Worldの内容を更新してから出力カメラを評価する。
   void RuntimeUpdate(float deltaTime);

   /// @brief シーン名と読み書き先の実ファイルを取得する。
   const SceneDefinition& GetDefinition() const { return definition_; }

   /// @brief シーン内容を管理するWorldを取得する。
   SceneWorld& GetSceneWorld() { return world_; }

   /// @brief シーン内容を読み取り専用で取得する。
   const SceneWorld& GetSceneWorld() const { return world_; }

private:
   SceneDefinition definition_;
   SceneWorld world_;
   bool initialized_ = false;
};
}
