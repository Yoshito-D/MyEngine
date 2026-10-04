#pragma once
#include <cmath>
#include <algorithm>

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Object/Component/UI/UIAnimationTypes.h"

namespace GameEngine {

/// @brief UITextComponentの不透明度を時間補間するコンポーネント
class UIFadeComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "UIFadeComponent";
   static constexpr ComponentDisplayName kDisplayName{ "UIフェード", "UI Fade" };

   /// @brief コンポーネントの型名を取得する
   /// @return 型名
   const char* GetTypeName() const override;

   /// @brief アタッチ時に自動再生設定を適用する
   void OnAttach() override;

   /// @brief 有効化時に自動再生設定を適用する
   void OnEnable() override;

   /// @brief 経過時間に応じて不透明度を更新する
   /// @param deltaTime 経過秒
   void Update(float deltaTime) override;

   /// @brief 現在位置から再生を続行する
   void Play();

   /// @brief 再生位置を保って一時停止する
   void Pause();

   /// @brief 開始不透明度へ戻して再生する
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

   /// @brief 不透明度の補間範囲と再生設定

   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings {
      /// @brief 不透明度の補間範囲と再生設定
      float startOpacity = 0.0f;
      float endOpacity = 1.0f;
      float delay = 0.0f;
      float duration = 0.5f;
      bool playOnEnable = true;
      UIPlaybackMode playbackMode = UIPlaybackMode::Once;
      UIEasingType easing = UIEasingType::EaseInOutSine;
   };
   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const {
      Settings settings;
      settings.startOpacity = startOpacity;
      settings.endOpacity = endOpacity;
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
      if (!std::isfinite(settings.startOpacity)) settings.startOpacity = defaults.startOpacity;
      if (!std::isfinite(settings.endOpacity)) settings.endOpacity = defaults.endOpacity;
      if (!std::isfinite(settings.delay)) settings.delay = defaults.delay;
      if (!std::isfinite(settings.duration)) settings.duration = defaults.duration;
      settings.startOpacity = std::clamp(settings.startOpacity, 0.0f, 1.0f);
      settings.endOpacity = std::clamp(settings.endOpacity, 0.0f, 1.0f);
      settings.duration = std::max(settings.duration, 0.001f);
      settings.delay = std::max(settings.delay, 0.0f);
      startOpacity = settings.startOpacity;
      endOpacity = settings.endOpacity;
      delay = settings.delay;
      duration = settings.duration;
      playOnEnable = settings.playOnEnable;
      playbackMode = settings.playbackMode;
      easing = settings.easing;
   }

private:
   float startOpacity = 0.0f;
   float endOpacity = 1.0f;
   float delay = 0.0f;
   float duration = 0.5f;
   bool playOnEnable = true;
   UIPlaybackMode playbackMode = UIPlaybackMode::Once;
   UIEasingType easing = UIEasingType::EaseInOutSine;

private:
   void Apply(float progress);

   float elapsed_ = 0.0f;
   bool playing_ = true;
};

} // namespace GameEngine
