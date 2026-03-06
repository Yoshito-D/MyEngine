#pragma once
#include <memory>
#include <vector>
#include "Utility/VectorMath.h"

namespace GameEngine {
   class Sprite;
   class Texture;
}

/// @brief タイマー表示クラス（テクスチャの数字0-9とコロンを使用）
class TimerDisplay {
public:
   TimerDisplay();
   ~TimerDisplay();

   /// @brief 初期化
   /// @param position 表示位置（スクリーン座標）
   /// @param digitSize 数字1つのサイズ
   void Initialize(const GameEngine::Vector2& position, const GameEngine::Vector2& digitSize);

   /// @brief タイマーの値を設定（秒単位）
   /// @param seconds 秒数（小数点以下は切り捨て）
   void SetValue(float seconds);
   
   /// @brief 更新処理（点滅アニメーション用）
   /// @param deltaTime デルタタイム
   void Update(float deltaTime);

   /// @brief 描画（DrawUIを使用）
   void Draw();

   /// @brief 位置を設定
   void SetPosition(const GameEngine::Vector2& position);

   /// @brief サイズを設定
   void SetDigitSize(const GameEngine::Vector2& size);

   /// @brief 現在の値を取得
   float GetValue() const { return currentValue_; }

private:
   /// @brief 数字テクスチャを読み込む
   void LoadTextures();

   /// @brief 表示する数字を計算（分:秒形式）
   void UpdateDigits();

   /// @brief 個別のスプライトを作成
   void CreateSprites();
   
   /// @brief 色の状態を更新
   void UpdateColorState();
   
   /// @brief スケールアニメーションを更新
   void UpdateScaleAnimation(float deltaTime);

private:
   // 表示位置とサイズ
   GameEngine::Vector2 position_;
   GameEngine::Vector2 digitSize_;

   // 現在の値（秒）
   float currentValue_;

   // 表示する数字（分の十の位、分の一の位、コロン、秒の十の位、秒の一の位）
   int digits_[5]; // [0]=分の十の位, [1]=分の一の位, [2]=コロン(10), [3]=秒の十の位, [4]=秒の一の位

   // スプライト（各桁用）
   std::vector<std::unique_ptr<GameEngine::Sprite>> sprites_;

   // テクスチャ（0-9とコロン）
   std::vector<GameEngine::Texture*> digitTextures_; // 0-9
   GameEngine::Texture* colonTexture_; // コロン
   
   // 点滅関連
   float blinkTimer_ = 0.0f;
   bool isVisible_ = true;
   static constexpr float kBlinkInterval_ = 0.1f; // 点滅間隔（秒）- 短くした
   
   // 色の状態
   enum class ColorState {
      Normal,  // 通常（白）
      Warning, // 警告（黄色）30秒以下
      Danger   // 危険（赤）10秒以下
   };
   ColorState colorState_ = ColorState::Normal;
   uint32_t currentColor_ = 0xffffffff; // 現在の色
   
   // スケールアニメーション関連
   float scaleAnimationTimer_ = 0.0f;
   float currentScale_ = 1.0f;
   static constexpr float kScaleAnimationInterval_ = 1.0f; // 1秒ごとにアニメーション
   static constexpr float kScaleAnimationDuration_ = 0.3f; // アニメーションの長さ
   static constexpr float kMinScale_ = 1.0f;
   static constexpr float kMaxScale_ = 1.2f;
};
