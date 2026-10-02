#pragma once
#include <xaudio2.h>
#include <wrl.h>
#include <vector>
#include <string>
#include <memory>

using namespace Microsoft::WRL;

namespace GameEngine {
class SoundClip;
/// @brief サウンドクラス
class Sound {
public:
   /// @brief 旧Soundラッパーが使用するボイスデバイスを設定する。デバイス終了前にnullptrを渡す。
   static void Initialize(IXAudio2* xAudio2);

   /// @brief デストラクタ
   ~Sound();

   /// @brief Mp3ファイルを読み込む
   /// @param filepath 読み込むMP3ファイルのパスを表すワイド文字列参照
   /// @note 失敗時は例外を送出し、読み込み前の音声と再生バッファを保持する
   void Load(const std::wstring& filepath);

   /// @brief 共有デコード済みクリップを読み込む。失敗時は以前のボイスを保持する。
   void LoadClip(std::shared_ptr<const SoundClip> clip);

   /// @brief 音声を再生
   /// @param volume 再生音量。デフォルトは1.0f
   /// @param loop ループ再生するかどうか。デフォルトはfalse
   void Play(float volume = 1.0f, bool loop = false, bool restart = true);

   /// @brief 音声を一時停止
   void Stop();

   /// @brief 音声を削除
   void Reset();

   /// @brief 音量を再設定
   /// @param volume 音量
   void SetVolume(float volume);

private:
   IXAudio2SourceVoice* sourceVoice_ = nullptr;
   std::shared_ptr<const SoundClip> clip_;
   bool isLooping_ = false;
   XAUDIO2_BUFFER buffer_{};

   bool isPlaying_ = false;
   bool isPaused_ = false;
};
}
