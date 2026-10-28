#pragma once

#include <cmath>
#include <algorithm>

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include <cstddef>
#include <cstdint>

namespace GameEngine {

/// @brief UITextComponentを先頭から1文字ずつ表示するコンポーネント
class TypewriterComponent final : public IObjectComponent {
public:
   /// @brief シーン保存とファクトリ検索で使うコンポーネント型名。
   static constexpr const char* kTypeName = "TypewriterComponent";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr ComponentDisplayName kDisplayName{"タイプライター", "Typewriter"};

   /// @brief コンポーネントの型名を取得する
   /// @return 型名
   const char* GetTypeName() const override;

   /// @brief アタッチ時に自動再生設定を適用する
   void OnAttach() override;

   /// @brief 有効化時に自動再生設定を適用する
   void OnEnable() override;

   /// @brief 経過時間に応じて表示文字数を更新する
   /// @param deltaTime 経過秒
   void Update(float deltaTime) override;

   /// @brief 現在位置から再生を続行する
   void Play();

   /// @brief 現在の表示文字数で一時停止する
   void Pause();

   /// @brief 文字を隠して先頭から再生する
   void Restart();

   /// @brief すべての文字を即時表示する
   void Complete();

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

   /// @brief 文字送り速度と再生設定

   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings {

      /// @brief 文字送り速度と再生設定
      float glyphsPerSecond = 20.0f;
      float delay = 0.0f;              ///< 再生開始から変化を始めるまでの待機時間（秒）。
      bool playOnEnable = true;        ///< コンポーネントが有効になるたびに自動再生するかどうか。
      bool loop = false;               ///< 終端到達後に再生を繰り返すかどうか。
      bool restartOnTextChange = true; ///< 文章が更新されたときに文字送りを先頭から再開するかどうか。
   };

   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const;

   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested);

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   float glyphsPerSecond = 20.0f;   ///< 文字送り速度と再生設定。
   float delay = 0.0f;              ///< 再生開始から変化を始めるまでの待機時間（秒）。
   bool playOnEnable = true;        ///< コンポーネントが有効になるたびに自動再生するかどうか。
   bool loop = false;               ///< 終端到達後に再生を繰り返すかどうか。
   bool restartOnTextChange = true; ///< 文章が更新されたときに文字送りを先頭から再開するかどうか。

private:
   size_t CountGlyphs() const;

   float elapsed_ = 0.0f;              ///< 処理を開始してからの経過時間（秒）。
   bool playing_ = true;               ///< 再生中か。
   uint64_t observedTextRevision_ = 0; ///< 文字送りが最後に確認した文章の改訂番号。
};

} // namespace GameEngine
