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
   /// @brief シーン保存とファクトリ検索で使うコンポーネント型名。
   static constexpr const char* kTypeName = "UITransformTweenComponent";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr ComponentDisplayName kDisplayName{"UIトゥイーン", "UI Transform Tween"};

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

   /// @copydoc GameEngine::ParameterComponent::GetSharedParameters
   nlohmann::json GetSharedParameters() const override;

   /// @copydoc GameEngine::ParameterComponent::ApplySharedParameters
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @brief エディタのインスペクターを描画する
   void DrawInspector() override;
#endif

   /// @brief 位置、拡縮、回転の補間範囲と再生設定

   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings {

      /// @brief 位置、拡縮、回転の補間範囲と再生設定
      bool animatePosition = true;
      bool animateScale = false;                          ///< 拡縮をアニメーションの対象に含めるかどうか。
      bool animateRotation = false;                       ///< 回転をアニメーションの対象に含めるかどうか。
      Vector2 startPosition = {0.0f, 0.0f};               ///< アニメーション開始時のUI位置。
      Vector2 endPosition = {0.0f, 0.0f};                 ///< アニメーション終了時のUI位置。
      Vector2 startScale = {1.0f, 1.0f};                  ///< アニメーション開始時または粒子生成時の拡大率。
      Vector2 endScale = {1.0f, 1.0f};                    ///< アニメーション終了時の拡大率。
      float startRotation = 0.0f;                         ///< アニメーション開始時または粒子生成時の回転。
      float endRotation = 0.0f;                           ///< アニメーション終了時の回転角。
      float delay = 0.0f;                                 ///< 再生開始から変化を始めるまでの待機時間（秒）。
      float duration = 0.5f;                              ///< 処理または再生が継続する時間（秒）。
      bool playOnEnable = true;                           ///< コンポーネントが有効になるたびに自動再生するかどうか。
      UIPlaybackMode playbackMode = UIPlaybackMode::Once; ///< 1回・繰り返し・往復などの再生方式。
      UIEasingType easing = UIEasingType::EaseOutCubic;   ///< 再生進行度を補間量へ変換するイージング方式。
   };

   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const;

   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested);

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   bool animatePosition = true;                        ///< 位置、拡縮、回転の補間範囲と再生設定。
   bool animateScale = false;                          ///< 拡縮をアニメーションの対象に含めるかどうか。
   bool animateRotation = false;                       ///< 回転をアニメーションの対象に含めるかどうか。
   Vector2 startPosition = {0.0f, 0.0f};               ///< アニメーション開始時のUI位置。
   Vector2 endPosition = {0.0f, 0.0f};                 ///< アニメーション終了時のUI位置。
   Vector2 startScale = {1.0f, 1.0f};                  ///< アニメーション開始時または粒子生成時の拡大率。
   Vector2 endScale = {1.0f, 1.0f};                    ///< アニメーション終了時の拡大率。
   float startRotation = 0.0f;                         ///< アニメーション開始時または粒子生成時の回転。
   float endRotation = 0.0f;                           ///< アニメーション終了時の回転角。
   float delay = 0.0f;                                 ///< 再生開始から変化を始めるまでの待機時間（秒）。
   float duration = 0.5f;                              ///< 処理または再生が継続する時間（秒）。
   bool playOnEnable = true;                           ///< コンポーネントが有効になるたびに自動再生するかどうか。
   UIPlaybackMode playbackMode = UIPlaybackMode::Once; ///< 1回・繰り返し・往復などの再生方式。
   UIEasingType easing = UIEasingType::EaseOutCubic;   ///< 再生進行度を補間量へ変換するイージング方式。

private:
   void Apply(float progress);

   float elapsed_ = 0.0f; ///< 処理を開始してからの経過時間（秒）。
   bool playing_ = true;  ///< 再生中か。
};

} // namespace GameEngine
