#pragma once
#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Audio/Audio.h"
#include <memory>
#include <string>
#include <vector>

namespace GameEngine {
/// @brief 再生可能なObject上の名前付きサウンドスロット。1コンポーネントで複数の音声を保持する。
class AudioSourceComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "AudioSourceComponent";
   static constexpr ComponentDisplayName kDisplayName{ "音声ソース", "Audio Source" };
   /// @brief 保存対象のスロット設定。クリップとハンドルは実行時専用。
   struct Slot {
      std::string name;
      std::string clipAssetId;
      AudioBus bus = AudioBus::Se;
      float volume = 1.0f;
      float pitch = 1.0f;
      bool loop = false;
      bool playOnStart = false;
   };
   /// @brief コンテナのDetach外で破棄された場合も、所有する再生を停止する。
   ~AudioSourceComponent() override;
   /// @brief シリアライズで使用する安定したコンポーネント型。
   const char* GetTypeName() const override { return kTypeName; }
   /// @brief 一意な名前のスロットを追加する。重複名または空名ならfalseを返す。
   bool AddSlot(Slot slot);
   /// @brief スロットを1つ削除し、その永続ボイスを停止する。
   bool RemoveSlot(const std::string& name);
   /// @brief 設定済みスロットを読み取り専用で取得する。
   const std::vector<Slot>& GetSlots() const { return slots_; }
   /// @brief 1つのスロットのクリップを置き換え、オート再生せずに準備する。
   bool SetClip(const std::string& name, const std::string& assetId);
   /// @brief すべてのクリップ参照を、再生開始せずに準備する。
   void Prepare();
   /// @brief 現在のランタイム有効化に対してplayOnStartを1回実行する。
   void BeginRuntime();
   /// @brief 名前付きの永続スロットを再生する。繰り返し呼び出しても重複再生しない。
   bool Play(const std::string& name);
   /// @brief 名前付きの永続スロットを先頭から再起動する。
   bool Restart(const std::string& name);
   /// @brief 名前付きスロットの永続ボイスだけを一時停止する。
   bool Pause(const std::string& name);
   /// @brief 名前付きスロットの永続ボイスだけを再開する。
   bool Resume(const std::string& name);
   /// @brief 名前付きスロットの永続ボイスだけを停止する。ワンショットは継続する。
   bool Stop(const std::string& name);
   /// @brief slot.loopに関係なく、独立した非ループボイスを再生する。
   AudioHandle PlayOneShot(const std::string& name);
   /// @brief このコンポーネントが所有する永続ボイスとワンショットを停止する。
   void StopAll();
   /// @brief 無効化されたとき、所有するボイスを停止する。
   void OnDisable() override { StopAll(); }
   /// @brief 切り離されたとき、所有するボイスを停止する。
   void OnDetach() override { StopAll(); }
   /// @brief クリップの解決だけを行い、編集中はオート再生しない。
   void OnReferencesChanged(SceneWorld&) override { Prepare(); }
   /// @brief クリップの解決だけを行い、ランタイム開始は後続の明示的なイベントで行う。
   void OnSceneLoaded(SceneWorld&) override { Prepare(); }
   /// @brief スロット設定だけを保存する。
   nlohmann::json Serialize() const override;
   /// @brief 有効なスロット設定を復元し、再生せずにクリップを準備する。
   void Deserialize(const nlohmann::json& data) override;
#ifdef MYPROJECT_NON_RELEASE
   /// @brief デバイス不要のテスト用に、名前付きスロットへデコード済みフィクスチャを注入する。
   void SetClipForTesting(const std::string& name, std::shared_ptr<const SoundClip> clip);
   /// @brief テストで名前付きスロットの永続ハンドルを確認する。
   AudioHandle GetHandleForTesting(const std::string& name) const;
#endif
#ifdef USE_IMGUI
   /// @brief スロットを編集し、プレビュー操作でシーンを変更済みにせず音声を確認する。
   void DrawInspector() override;
#endif
private:
   struct RuntimeSlot { std::shared_ptr<const SoundClip> clip; AudioHandle handle; };
   size_t FindSlot(const std::string& name) const;
   std::vector<Slot> slots_;
   std::vector<RuntimeSlot> runtime_;
   std::vector<AudioHandle> oneShots_;
   bool runtimeStarted_ = false;
};
}
