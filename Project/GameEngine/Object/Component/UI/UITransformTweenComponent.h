#pragma once

#include "Component/IObjectComponent.h"
#include "UIAnimationTypes.h"
#include "Utility/VectorMath.h"

namespace GameEngine {

/// @brief UIの移動、拡縮、回転を時間補間するコンポーネント
class UITransformTweenComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "UITransformTweenComponent";
   static constexpr ComponentDisplayName kDisplayName{ "UIトゥイーン", "UI Transform Tween" };

   /// @brief コンポーネントの型名を取得する
   /// @return 型名
   const char* GetTypeName() const override;

   /// @brief アタッチ時に自動再生設定を適用する
   void OnAttach() override;

   /// @brief 有効化時に自動再生設定を適用する
   void OnEnable() override;

   /// @brief 経過時間に応じてTransformを更新する
   /// @param deltaTime 経過秒
   void Update(float deltaTime) override;

   /// @brief 現在位置から再生を続行する
   void Play();

   /// @brief 再生位置を保って一時停止する
   void Pause();

   /// @brief 開始値へ戻して再生する
   void Restart();

   /// @brief 再生中かを取得する
   /// @return 再生中ならtrue
   bool IsPlaying() const { return playing_; }

   /// @brief 設定をJSON化する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONから設定を復元する
   /// @param data 保存済みJSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief エディタのインスペクターを描画する
   void DrawInspector() override;
#endif

   /// @brief 位置、拡縮、回転の補間範囲と再生設定
   bool animatePosition = true;
   bool animateScale = false;
   bool animateRotation = false;
   Vector2 startPosition = { 0.0f, 0.0f };
   Vector2 endPosition = { 0.0f, 0.0f };
   Vector2 startScale = { 1.0f, 1.0f };
   Vector2 endScale = { 1.0f, 1.0f };
   float startRotation = 0.0f;
   float endRotation = 0.0f;
   float delay = 0.0f;
   float duration = 0.5f;
   bool playOnEnable = true;
   UIPlaybackMode playbackMode = UIPlaybackMode::Once;
   UIEasingType easing = UIEasingType::EaseOutCubic;

private:
   void Apply(float progress);

   float elapsed_ = 0.0f;
   bool playing_ = true;
};

} // namespace GameEngine
