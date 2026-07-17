#pragma once

#include "Component/IObjectComponent.h"
#include "UIAnimationTypes.h"

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
