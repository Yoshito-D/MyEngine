#pragma once

#include <cmath>
#include <algorithm>
#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Audio/SoundClip.h"
#include <memory>
#include <string>

namespace GameEngine {

/// @brief 1シーン分のBGM要求。再生はAudioのBgmPlayerが所有する。
class BgmComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "BgmComponent";          ///< シリアライズで使用する安定した型名。
   static constexpr ComponentDisplayName kDisplayName{"BGM", "BGM"}; ///< エディターに表示する日本語名と英語名。

   /// @brief このコンポーネントのBGM要求を解放する。
   ~BgmComponent() override;

   /// @brief シリアライズで使用する安定した型名。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 設定されたクリップを再生せずに準備する。
   void Prepare();

   /// @brief 実際のランタイム開始時にplayOnStartを1回だけ送信する。
   bool BeginRuntime();

   /// @brief このコンポーネントに再生可能な起動時トラックが設定されているかどうか。
   bool HasStartupTrack() const { return IsEnabled() && playOnStart && clip_ != nullptr; }

   /// @brief 設定されたBGMを明示的に要求する。同じトラックは再起動しない。
   bool Play();

   /// @brief 設定されたBGMを先頭から明示的に再起動する。
   bool Restart();

   /// @brief このコンポーネントが現在所有するBGM要求だけを解放する。
   void Stop();

   /// @brief 無効化されたとき、所有するBGMを停止する。
   void OnDisable() override { Stop(); }

   /// @brief 後続シーンの要求に影響を与えず、所有権だけを取り除く。
   void OnDetach() override { Stop(); }

   /// @brief クリップの解決だけを行う。編集時に音楽は開始しない。
   void OnReferencesChanged(SceneWorld&) override { Prepare(); }

   /// @brief クリップの解決だけを行う。音楽は後続のランタイムイベントで開始する。
   void OnSceneLoaded(SceneWorld&) override { Prepare(); }

   /// @brief 設定だけを保存し、ボイスやフェード状態は保存しない。
   nlohmann::json Serialize() const override;

   /// @brief 検証済みの設定を復元し、再生せずにクリップを準備する。
   void Deserialize(const nlohmann::json& data) override;
#ifdef MYPROJECT_NON_RELEASE

   /// @brief デバイス不要のテスト用に、アセットID付きのデコード済みフィクスチャを注入する。
   void SetClipForTesting(const std::string& assetId, std::shared_ptr<const SoundClip> clip);
#endif
#ifdef USE_IMGUI

   /// @brief BGM設定を編集し、独立したエディタープレビュー操作を呼び出す。
   void DrawInspector() override;
#endif

   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings {
      std::string clipAssetId;           ///< 再生する音声クリップの永続アセットID。
      float volume = 1.0f;               ///< 個別の音量倍率。
      bool loop = true;                  ///< 終端到達後に再生を繰り返すかどうか。
      bool playOnStart = true;           ///< シーンの実行開始時に自動再生するかどうか。
      float fadeInSeconds = 0.0f;        ///< 音量を立ち上げる時間（秒）。
      float fadeOutSeconds = 0.0f;       ///< 音量を下げて停止する時間（秒）。
      bool continueAcrossScenes = false; ///< シーン切り替え後も再生を継続するかどうか。
   };

   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const;

   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested);

private:
   std::string clipAssetId;           ///< 再生する音声クリップの永続アセットID。
   float volume = 1.0f;               ///< 個別の音量倍率。
   bool loop = true;                  ///< 終端到達後に再生を繰り返すかどうか。
   bool playOnStart = true;           ///< シーンの実行開始時に自動再生するかどうか。
   float fadeInSeconds = 0.0f;        ///< 音量を立ち上げる時間（秒）。
   float fadeOutSeconds = 0.0f;       ///< 音量を下げて停止する時間（秒）。
   bool continueAcrossScenes = false; ///< シーン切り替え後も再生を継続するかどうか。

private:
   bool Submit(bool restart);
   std::shared_ptr<const SoundClip> clip_; ///< 再生対象の音声またはアニメーションクリップへの参照。
   std::string preparedAssetId_;           ///< 再生開始に備えて読み込んだ音声アセットID。
   uint64_t ownerToken_ = 0;               ///< 再生要求元を識別する所有者トークン。
   bool runtimeStarted_ = false;           ///< 現在の実行セッションで自動再生処理を開始済みかどうか。
};
}
