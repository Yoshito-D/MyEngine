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
   static constexpr const char* kTypeName = "BgmComponent";
   static constexpr ComponentDisplayName kDisplayName{ "BGM", "BGM" };
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
      std::string clipAssetId;
      float volume = 1.0f;
      bool loop = true;
      bool playOnStart = true;
      float fadeInSeconds = 0.0f;
      float fadeOutSeconds = 0.0f;
      bool continueAcrossScenes = false;
   };
   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const {
      Settings settings;
      settings.clipAssetId = clipAssetId;
      settings.volume = volume;
      settings.loop = loop;
      settings.playOnStart = playOnStart;
      settings.fadeInSeconds = fadeInSeconds;
      settings.fadeOutSeconds = fadeOutSeconds;
      settings.continueAcrossScenes = continueAcrossScenes;
      return settings;
   }
   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested) {
      auto settings = requested;
      [[maybe_unused]] const Settings defaults;
      if (!std::isfinite(settings.volume)) settings.volume = defaults.volume;
      if (!std::isfinite(settings.fadeInSeconds)) settings.fadeInSeconds = defaults.fadeInSeconds;
      if (!std::isfinite(settings.fadeOutSeconds)) settings.fadeOutSeconds = defaults.fadeOutSeconds;
      settings.volume = std::clamp(settings.volume, 0.0f, 1.0f);
      settings.fadeInSeconds = std::max(settings.fadeInSeconds, 0.0f);
      settings.fadeOutSeconds = std::max(settings.fadeOutSeconds, 0.0f);
      clipAssetId = settings.clipAssetId;
      volume = settings.volume;
      loop = settings.loop;
      playOnStart = settings.playOnStart;
      fadeInSeconds = settings.fadeInSeconds;
      fadeOutSeconds = settings.fadeOutSeconds;
      continueAcrossScenes = settings.continueAcrossScenes;
      Prepare();
   }

private:
   std::string clipAssetId;
   float volume = 1.0f;
   bool loop = true;
   bool playOnStart = true;
   float fadeInSeconds = 0.0f;
   float fadeOutSeconds = 0.0f;
   bool continueAcrossScenes = false;

private:
   bool Submit(bool restart);
   std::shared_ptr<const SoundClip> clip_;
   std::string preparedAssetId_;
   uint64_t ownerToken_ = 0;
   bool runtimeStarted_ = false;
};
}
