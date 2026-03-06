#pragma once
#include "Scene/BaseScene.h"
#include "Object/Sprite/Sprite.h"
#include "Core/Input/KeyConfig.h"
#include <memory>

namespace GameEngine { class Model; }

/// @brief ゲームオーバーシーン
class GameOverScene : public GameEngine::BaseScene {
public:
   GameOverScene();

   /// @brief 初期化
   void Initialize() override;

   /// @brief 更新
   void Update() override;

   /// @brief 描画
   void Draw() override;

   void Finalize() override;

private:
   enum class SelectionState {
      Left,   // ゲームシーンに戻る（リトライ）
      Right   // タイトルシーンに戻る
   };

   void UpdateSelection();
   void UpdateSpriteScales();
   void HandleDecision();

   float elapsedTime_ = 0.0f;
   float animationTime_ = 0.0f;  // アニメーション用の時間
   bool hasRequestedTransition_ = false;
   
   SelectionState currentSelection_ = SelectionState::Left;  // デフォルトは左（リトライ）
   
   // スプライト
   std::unique_ptr<GameEngine::Sprite> topSprite_ = nullptr;       // 上のスプライト
   std::unique_ptr<GameEngine::Sprite> leftSprite_ = nullptr;      // 左下のスプライト（リトライ）
   std::unique_ptr<GameEngine::Sprite> rightSprite_ = nullptr;     // 右下のスプライト（タイトル）
   std::unique_ptr<GameEngine::Sprite> tipsSprite_ = nullptr;     // 右下のスプライト（タイトル）
   
   // キーコンフィグ
   std::unique_ptr<GameEngine::KeyConfig> keyConfig_ = nullptr;
   
   // スケールアニメーション
   float baseScale_ = 100.0f;          // 基本スケール
   float minSelectedScale_ = 0.9f;     // 選択中の最小スケール
   float maxSelectedScale_ = 1.0f;     // 選択中の最大スケール
   float unselectedScale_ = 0.8f;      // 非選択時のスケール
   float scaleFrequency_ = 3.0f;       // スケール変化の周波数
   
   // ボタン押下時のリアクション
   bool isPressingButton_ = false;
   float pressAnimationTime_ = 0.0f;
   float pressAnimationDuration_ = 0.3f;  // ボタン押下アニメーション時間を0.3秒に延長
   float pressScaleMultiplier_ = 1.5f;    // ボタン押下時のスケール倍率を1.5倍に増加
   
   // スカイドームとサン
   std::unique_ptr<GameEngine::Model> skyDomeModel_ = nullptr;
   std::unique_ptr<GameEngine::Model> sunModel_ = nullptr;
   float cameraRotationSpeed_ = 0.1f;  // カメラ回転速度

   // Tips表示用の静的変数（シーンごとに切り替わる）
   static int sTipsIndex_;
   int currentTipsIndex_ = 0;  // 現在のインスタンスで使用するTipsのインデックス
};
