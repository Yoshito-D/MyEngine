#pragma once
#include "GameEngine/Audio/SoundClip.h"
#include <xaudio2.h>
#include <wrl.h>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace GameEngine {
class BgmPlayer;

/// @brief 再生カテゴリ。実効音量はMaster * category * voice * fadeで決まる。
enum class AudioBus { Bgm, Se, Ui };

/// @brief 世代番号で検証する再生識別子。完了またはRelease後は無効になる。
struct AudioHandle {
   uint32_t index = UINT32_MAX;
   uint32_t generation = 0;
   /// @brief 構文上有効なインデックスを持つかどうか。
   explicit operator bool() const { return index != UINT32_MAX; }
};

/// @brief 所有中のハンドルから取得できる再生状態。
enum class AudioPlaybackState { Stopped, Playing, Paused };

/// @brief オーディオデバイス、アクティブなボイス、シーンBGMプレイヤーの所有者。
class Audio {
public:
   /// @brief Initializeが呼ばれるまで無音状態で動作するオーディオサービスを構築する。
   Audio();
   /// @brief 所有者がFinalizeを呼ばなかった場合にデバイスリソースを解放する。
   ~Audio();
   /// @brief Media FoundationとXAudio2を初期化する。失敗時は無音状態を維持する。
   void Initialize();
   /// @brief デバイスとMedia Foundationの終了前に全ボイスを解放する。複数回呼び出しても安全。
   void Finalize();
   /// @brief 旧Soundラッパー専用にデバイスを返す。
   IXAudio2* GetXAudio2() const { return xAudio2_.Get(); }
   /// @brief 再生デバイスが作成済みかどうか。
   bool IsAvailable() const { return masteringVoice_ != nullptr || testMode_; }

   /// @brief ボイスを確保して再生を開始する。無音中、一時停止中、または上限時は無効値を返す。
   AudioHandle Create(std::shared_ptr<const SoundClip> clip, AudioBus bus, float volume = 1.0f,
      float pitch = 1.0f, bool loop = false, bool preview = false);
   /// @brief 停止中のハンドルを先頭から再生する。再生中・一時停止中のハンドルは変更しない。
   bool Play(AudioHandle handle);
   /// @brief 再生中の場合も含め、ハンドルを先頭から再起動する。
   bool Restart(AudioHandle handle);
   /// @brief ハンドルを現在位置で一時停止する。
   bool Pause(AudioHandle handle);
   /// @brief 明示的に一時停止したハンドルを現在位置から再開する。
   bool Resume(AudioHandle handle);
   /// @brief ハンドルを停止してバッファを破棄する。後続のPlayは先頭から開始する。
   bool Stop(AudioHandle handle);
   /// @brief ハンドルとPCM参照を解放する。古いコピーから新しいボイスへアクセスすることはできない。
   void Release(AudioHandle handle);
   /// @brief 独立した非ループボイスを作成する。
   AudioHandle PlayOneShot(std::shared_ptr<const SoundClip> clip, AudioBus bus,
      float volume = 1.0f, float pitch = 1.0f, bool preview = false);
   /// @brief ボイス個別の音量を設定する。
   bool SetVolume(AudioHandle handle, float volume);
   /// @brief ボイスのピッチ倍率を設定する。
   bool SetPitch(AudioHandle handle, float pitch);
   /// @brief ボイスを別のカテゴリへ移し、音量を即時更新する。
   bool SetBus(AudioHandle handle, AudioBus bus);
   /// @brief ボイスのフェード係数を0から1の範囲で設定する。
   bool SetFade(AudioHandle handle, float fade);
   /// @brief Master、BGM、SE、UIの音量を設定し、既存ボイスを即時更新する。
   void SetBusVolume(AudioBus bus, float volume);
   /// @brief Master音量を設定し、既存ボイスを即時更新する。
   void SetMasterVolume(float volume);
   /// @brief 個別に一時停止したボイスを再開せず、ゲーム音声全体を一時停止・再開する。
   void SetGamePaused(bool paused);
   /// @brief エディタープレビューだけを残し、ゲーム用の全ボイスを停止・解放する。
   void StopGameAudio();
   /// @brief エディタープレビュー用の全ボイスを停止・解放する。
   void StopPreviews();
   /// @brief 完了したボイスを回収し、実時間秒でBGMのフェードを進める。
   void Update(float realSeconds);
   /// @brief ハンドルが再生エントリを引き続き所有しているかどうか。
   bool IsValid(AudioHandle handle) const;
   /// @brief ハンドルの状態を返す。完了または解放後はnulloptを返す。
   std::optional<AudioPlaybackState> GetState(AudioHandle handle) const;
   /// @brief 現在確保されているソースボイス数。
   size_t GetVoiceCount() const;
   /// @brief このオーディオサービスが所有するBGMコントローラー。
   BgmPlayer& GetBgmPlayer();
#ifdef MYPROJECT_NON_RELEASE
   /// @brief 決定的な音声状態テスト用に、デバイス不要のバックエンドを開始する。
   void InitializeForTesting();
   /// @brief 非ループのテストボイスが自然終了した状態を再現する。
   void CompleteForTesting(AudioHandle handle);
   /// @brief デバイス不要のテストで実効音量を確認する。
   float GetEffectiveVolumeForTesting(AudioHandle handle) const;
   /// @brief テストでPlayとRestartを区別するため、開始回数を取得する。
   uint32_t GetStartCountForTesting(AudioHandle handle) const;
#endif

private:
   using State = AudioPlaybackState;
   struct Entry {
      uint32_t generation = 1;
      std::shared_ptr<const SoundClip> clip;
      IXAudio2SourceVoice* voice = nullptr;
      XAUDIO2_BUFFER buffer{};
      AudioBus bus = AudioBus::Se;
      State state = State::Stopped;
      float volume = 1.0f, pitch = 1.0f, fade = 1.0f;
      bool loop = false, preview = false, pausedByGame = false;
      bool testVoice = false;
      uint32_t startCount = 0;
   };
   static constexpr size_t kMaxVoices = 64;
   static constexpr size_t kMaxHandles = 256;
   Entry* Find(AudioHandle handle);
   const Entry* Find(AudioHandle handle) const;
   bool Start(Entry& entry);
   void DestroyVoice(Entry& entry);
   void ApplyVolume(Entry& entry);
   void Retire(Entry& entry);

   Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
   IXAudio2MasteringVoice* masteringVoice_ = nullptr;
   bool mfStarted_ = false;
   bool gamePaused_ = false;
   bool testMode_ = false;
   float masterVolume_ = 1.0f;
   std::array<float, 3> busVolumes_{ 1.0f, 1.0f, 1.0f };
   std::array<Entry, kMaxHandles> entries_{};
   std::unique_ptr<BgmPlayer> bgmPlayer_;
};
}
