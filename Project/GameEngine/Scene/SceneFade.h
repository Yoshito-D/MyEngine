#pragma once
#include <memory>
#include "Utility/VectorMath.h"

namespace GameEngine {
class Sprite;
class Texture;
class Material;
class Camera;

/// @brief シーンフェード管理基底クラス
class SceneFade {
public:
   enum class FadeState {
	  None,      // フェードなし
	  FadeIn,    // フェードイン中
	  FadeOut    // フェードアウト中
   };

   enum class EasingType {
	  Linear,      // 線形
	  EaseIn,      // イーズイン
	  EaseOut,     // イーズアウト
	  EaseInOut    // イーズインアウト
   };

   SceneFade() = default;
   virtual ~SceneFade() = default;

   /// @brief フェードの初期化
   /// @param uiCamera UIカメラ
   /// @param fadeDuration フェード時間（秒）
   /// @param fadeColor フェードカラー（デフォルトは黒）
   virtual void Initialize(float fadeDuration = 1.5f, uint32_t fadeColor = 0x000000ff);

   /// @brief フェードの更新（デルタタイムは内部でEngineContextから取得）
   virtual void Update();

   /// @brief フェードの描画
   virtual void Draw();

   /// @brief フェードインを開始
   virtual void StartFadeIn();

   /// @brief フェードアウトを開始
   virtual void StartFadeOut();

   /// @brief フェードが完了したかを取得
   /// @return フェード完了状態
   bool IsCompleted() const { return fadeState_ == FadeState::None; }

   /// @brief フェードアウトが完了したかを取得
   /// @return フェードアウト完了状態
   bool IsFadeOutCompleted() const { return isFadeOutCompleted_; }

   /// @brief フェードアウト完了フラグをリセット
   void ResetFadeOutCompleted() { isFadeOutCompleted_ = false; }

   /// @brief フェードの状態を取得
   /// @return フェードの状態
   FadeState GetFadeState() const { return fadeState_; }

   /// @brief フェード時間を設定
   /// @param duration フェード時間（秒）
   void SetFadeDuration(float duration) { fadeDuration_ = duration; }

   /// @brief フェードカラーを設定
   /// @param color フェードカラー
   virtual void SetFadeColor(uint32_t color);

   /// @brief イージングタイプを設定
   /// @param type イージングタイプ
   void SetEasingType(EasingType type) { easingType_ = type; }

   /// @brief イージングパワーを設定
   /// @param power イージングパワー（デフォルト: 2.0）
   void SetEasingPower(float power) { easingPower_ = power; }

protected:
   std::unique_ptr<Sprite> fadeSprite_ = nullptr;
   Material* fadeMaterial_ = nullptr;
   Texture* whiteTexture_ = nullptr;

   FadeState fadeState_ = FadeState::None;
   float fadeTimer_ = 0.0f;
   float fadeDuration_ = 1.5f;
   float fadeAlpha_ = 0.0f;
   uint32_t fadeColor_ = 0x000000ff;
   bool isFadeOutCompleted_ = false;

   EasingType easingType_ = EasingType::EaseInOut;
   float easingPower_ = 2.0f;
};
}
