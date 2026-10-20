#pragma once

#include "GameEngine/Scene/BaseScene.h"
#include "GameEngine/Scene/SceneWorld.h"
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

private:
   std::filesystem::path sceneFilePath_;
   bool isLoaded_ = false;
};

} // namespace GameEngine
