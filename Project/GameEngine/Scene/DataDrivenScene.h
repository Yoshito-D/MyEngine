#pragma once

#include "BaseScene.h"
#include "SceneWorld.h"
#include <filesystem>

namespace GameEngine {

/// @brief JSON定義だけで構築される汎用シーン
class DataDrivenScene final : public BaseScene {
public:
   /// @brief 読み込むJSONファイルを指定して汎用シーンを作成する
   /// @param sceneFilePath シーンJSONへのパス
   explicit DataDrivenScene(std::filesystem::path sceneFilePath);

   /// @brief JSONからSceneWorldを生成する
   void LoadSceneDataIfNeeded() override;

   /// @brief このシーンが所有するSceneWorldを取得する
   /// @return SceneWorldへの参照
   SceneWorld& GetSceneWorld() { return sceneWorld_; }

   /// @brief このシーンが所有するSceneWorldを取得する
   /// @return SceneWorldへの読み取り専用参照
   const SceneWorld& GetSceneWorld() const { return sceneWorld_; }

protected:
   /// @brief 汎用オブジェクトのコンポーネントを更新する
   /// @param deltaTime ゲーム用デルタタイム（秒）
   void OnUpdate(float deltaTime) override;

   /// @brief JSONから生成したシーン要素を破棄する
   void OnFinalize() override;

private:
   std::filesystem::path sceneFilePath_;
   SceneWorld sceneWorld_;
   bool isLoaded_ = false;
};

} // namespace GameEngine
