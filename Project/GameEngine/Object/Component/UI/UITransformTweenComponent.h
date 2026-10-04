#pragma once
#include <cmath>
#include <algorithm>

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Object/Component/UI/UIAnimationTypes.h"
#include "GameEngine/Math/VectorMath.h"

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

   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings {
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
   };
   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const {
      Settings settings;
      settings.animatePosition = animatePosition;
      settings.animateScale = animateScale;
      settings.animateRotation = animateRotation;
      settings.startPosition = startPosition;
      settings.endPosition = endPosition;
      settings.startScale = startScale;
      settings.endScale = endScale;
      settings.startRotation = startRotation;
      settings.endRotation = endRotation;
      settings.delay = delay;
      settings.duration = duration;
      settings.playOnEnable = playOnEnable;
      settings.playbackMode = playbackMode;
      settings.easing = easing;
      return settings;
   }
   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested) {
      auto settings = requested;
      [[maybe_unused]] const Settings defaults;
      if (!std::isfinite(settings.startPosition.x)) settings.startPosition.x = defaults.startPosition.x;
      if (!std::isfinite(settings.startPosition.y)) settings.startPosition.y = defaults.startPosition.y;
      if (!std::isfinite(settings.endPosition.x)) settings.endPosition.x = defaults.endPosition.x;
      if (!std::isfinite(settings.endPosition.y)) settings.endPosition.y = defaults.endPosition.y;
      if (!std::isfinite(settings.startScale.x)) settings.startScale.x = defaults.startScale.x;
      if (!std::isfinite(settings.startScale.y)) settings.startScale.y = defaults.startScale.y;
      if (!std::isfinite(settings.endScale.x)) settings.endScale.x = defaults.endScale.x;
      if (!std::isfinite(settings.endScale.y)) settings.endScale.y = defaults.endScale.y;
      if (!std::isfinite(settings.startRotation)) settings.startRotation = defaults.startRotation;
      if (!std::isfinite(settings.endRotation)) settings.endRotation = defaults.endRotation;
      if (!std::isfinite(settings.delay)) settings.delay = defaults.delay;
      if (!std::isfinite(settings.duration)) settings.duration = defaults.duration;
      settings.duration = std::max(settings.duration, 0.001f);
      settings.delay = std::max(settings.delay, 0.0f);
      animatePosition = settings.animatePosition;
      animateScale = settings.animateScale;
      animateRotation = settings.animateRotation;
      startPosition = settings.startPosition;
      endPosition = settings.endPosition;
      startScale = settings.startScale;
      endScale = settings.endScale;
      startRotation = settings.startRotation;
      endRotation = settings.endRotation;
      delay = settings.delay;
      duration = settings.duration;
      playOnEnable = settings.playOnEnable;
      playbackMode = settings.playbackMode;
      easing = settings.easing;
   }

private:
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
