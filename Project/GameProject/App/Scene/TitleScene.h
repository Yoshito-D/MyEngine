#pragma once
#include "BaseScene.h"
#include <memory>
#include "Sprite/Sprite.h"
#include "Graphics/Texture.h"
#include "Camera/Camera.h"

namespace GameEngine { class Model; }

/// @brief タイトルシーン
class TitleScene : public GameEngine::BaseScene {
public:
   TitleScene();
   
   /// @brief 初期化
   void Initialize() override;
   
   /// @brief 更新
   void Update() override;
   
   /// @brief 描画
   void Draw() override;
   
   void Finalize() override;

private:
   std::unique_ptr<GameEngine::Sprite> titleSprite_ = nullptr;
   std::unique_ptr<GameEngine::Sprite> pressSpaceSprite_ = nullptr;
   std::unique_ptr<GameEngine::Model> skyDomeModel_ = nullptr;

   std::unique_ptr<GameEngine::Model> sunModel_ = nullptr;
   
   float blinkTimer_ = 0.0f;  // 点滅用タイマー
   bool showPressSpace_ = true;
   
   // ボタン押下時のアニメーション
   bool isButtonPressed_ = false;
   float pressAnimationTime_ = 0.0f;
   float normalBlinkInterval_ = 0.5f;  // 通常の点滅間隔
   float fastBlinkInterval_ = 0.01f;    // 速い点滅間隔
   float pressAnimationDuration_ = 1.0f; // ボタン押下アニメーション時間
};
