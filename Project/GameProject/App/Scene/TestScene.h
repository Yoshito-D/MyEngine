#pragma once
#include "BaseScene.h"
#include "Model.h"
#include <memory>

/// @brief ライトテスト用シーン
class TestScene : public GameEngine::BaseScene {
public:
   /// @brief 初期化
   void Initialize() override;

   /// @brief 更新
   void Update() override;

   /// @brief 描画
   void Draw() override;

private:
   std::unique_ptr<GameEngine::Model> testSpherePhongModel_ = nullptr;
   std::unique_ptr<GameEngine::Model> testSphereBlinnPhongModel_ = nullptr;
   std::unique_ptr<GameEngine::Model> testPlaneModel_ = nullptr;
   std::unique_ptr<GameEngine::Model> testCubeGltfModel_ = nullptr;
};
