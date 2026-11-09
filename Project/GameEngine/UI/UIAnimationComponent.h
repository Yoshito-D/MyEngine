#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Object/Component/UI/UIAnimationTypes.h"
#include "GameEngine/Math/VectorMath.h"
#include <array>
#include <cstdint>
#include <vector>

namespace GameEngine {

/// @brief UIの操作や画面の開閉に対応する再生タイミング。
enum class UIAnimationEvent : std::int32_t {
   Show,
   Hide,
   Focus,
   Unfocus,
   Click,
   Idle,
   Count
};

/// @brief インスペクターから選べる演出の雛形。
enum class UIAnimationPreset : std::int32_t {
   None,
   Fade,
   SlideLeft,
   SlideRight,
   SlideUp,
   SlideDown,
   Pop,
   Emphasis,
   Press,
   Pulse,
   Float,
   Shake,
   Spin,
   Count
};

/// @brief 保存された配置へ加算・乗算する演出値。位置はUIピクセル、回転はrad。
struct UIAnimationPose {
   Vector2 offset{};
   Vector2 scale{1.0f, 1.0f};
   float rotation = 0.0f;
   float opacity = 1.0f;
};

/// @brief クリップ内の正規化時刻と演出値。timeは0～1。
struct UIAnimationKeyframe {
   float time = 0.0f;
   UIAnimationPose pose{};
};

/// @brief 一つのタイミングで再生するキーフレーム列。
struct UIAnimationClip {
   bool enabled = false;
   float duration = 0.2f; ///< 片道の再生時間（秒）。
   float delay = 0.0f;    ///< 開始待機時間（秒）。
   UIEasingType easing = UIEasingType::EaseOutCubic;
   UIPlaybackMode playback = UIPlaybackMode::Once;
   std::vector<UIAnimationKeyframe> keys{{0.0f, {}}, {1.0f, {}}};
};

/// @brief キーフレームからUI演出値を計算する。保存された配置やマテリアルは書き換えない。
class UIAnimationComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "UIAnimationComponent";
   static constexpr ComponentDisplayName kDisplayName{"UIアニメーション", "UI Animation"};
   static constexpr size_t kEventCount = static_cast<size_t>(UIAnimationEvent::Count);

   /// @brief イベントごとの編集設定。再生位置やプレビューは保存しない。
   struct Settings {
      std::array<UIAnimationClip, kEventCount> clips{};
   };

   /// @brief 表示・非表示のフェードを持つ初期設定で構築する。
   UIAnimationComponent();

   /// @brief 保存と検索に使う型名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief ポーズ中も実時間で進むUI更新区分を返す。
   ObjectUpdateDomain GetUpdateDomain() const override { return ObjectUpdateDomain::UI; }

   /// @brief 編集設定のコピーを返す。
   Settings DescribeSettings() const { return settings_; }

   /// @brief キーフレームを検証・整列して設定し、再生とプレビューをリセットする。
   void Configure(const Settings& settings);

   /// @brief イベントに適したプリセットを生成する。
   static UIAnimationClip MakePreset(UIAnimationPreset preset, UIAnimationEvent event);

   /// @brief 表示・非表示のフェードとポップをまとめて設定する。
   void ApplyScreenPreset();

   /// @brief 選択時の拡大・選択解除・決定時の押下演出をまとめて設定する。
   void ApplyButtonPreset();

   /// @brief 指定タイミングの演出を再生する。無効な設定ではfalse。
   bool Play(UIAnimationEvent event);

   /// @brief 再生とプレビューを止め、追加の変形と不透明度を基準値へ戻す。
   void Reset();

   /// @brief 現在の演出値を返す。editing=trueでは明示的なプレビューだけを反映する。
   UIAnimationPose GetPose(bool editing = false) const;

   /// @brief 指定したタイミングの演出が進行中かを返す。
   bool IsPlaying(UIAnimationEvent event) const;

   /// @brief 画面の表示を通知する。終了演出からの再表示では現在値からつなぐ。
   void NotifyShown();

   /// @brief 画面の非表示開始を通知し、終了演出中の選択通知を抑止する。
   void NotifyHiding();

   /// @brief 選択・ホバーの変化を通知し、選択演出を現在値から切り替える。
   void SetSelected(bool selected);

   /// @brief 表示開始時に初期設定の演出を準備する。
   void OnAttach() override;

   /// @brief 再有効化時に表示演出を準備する。
   void OnEnable() override;

   /// @brief 無効化時に一時的な演出値を解除する。
   void OnDisable() override;

   /// @brief シーン初期化とPlay開始時に再生状態を初期化する。
   void OnSceneLoaded(SceneWorld&) override;

   /// @brief UI実時間（秒）で再生を進める。
   void Update(float deltaTime) override;

   /// @brief 指定した秒数のプレビューを開始またはスクラブする。
   void Preview(UIAnimationEvent event, float seconds = 0.0f, bool playing = true);

   /// @brief 編集モードの実時間（秒）でプレビューだけを進める。
   void UpdatePreview(float deltaTime);

   /// @brief プレビューを解除し、保存された配置の表示へ戻す。
   void StopPreview();

   /// @brief 編集設定だけを保存する。
   nlohmann::json Serialize() const override;

   /// @brief 保存設定から演出を復元する。実行状態は初期化する。
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief プリセット・キーフレーム・再生位置を既存インスペクターで編集する。
   void DrawInspector() override;
#endif

private:
   enum class Channel : std::int32_t {
      Visibility,
      Selection,
      Feedback,
      Ambient,
      Count
   };

   struct Player {
      UIAnimationEvent event = UIAnimationEvent::Count;
      UIAnimationClip clip{};
      UIAnimationPose pose{};
      UIAnimationPose start{};
      float elapsed = 0.0f;
      bool playing = false;
      bool compensateStart = false;
   };

   static Channel GetChannel(UIAnimationEvent event);
   static UIAnimationPose Sample(const UIAnimationClip& clip, float elapsed);
   static void Advance(Player& player, float deltaTime);
   bool Start(UIAnimationEvent event, bool fromCurrent = false);
   Settings settings_{};
   std::array<Player, static_cast<size_t>(Channel::Count)> players_{};
   bool selected_ = false;
   bool hiding_ = false;
   bool previewActive_ = false;
   bool previewPlaying_ = false;
   float previewTime_ = 0.0f;
   UIAnimationEvent previewEvent_ = UIAnimationEvent::Show;
};
}
