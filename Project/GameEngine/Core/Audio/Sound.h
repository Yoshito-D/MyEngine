#pragma once
#include <xaudio2.h>
#include <wrl.h>
#include <vector>
#include <string>

using namespace Microsoft::WRL;

namespace GameEngine {
/// @brief サウンドクラス
class Sound {
public:
   static void Initialize(IXAudio2* xAudio2);

   /// @brief デストラクタ
   ~Sound();

   /// @brief Mp3ファイルを読み込む
   /// @param filepath 読み込むMP3ファイルのパスを表すワイド文字列参照
   /// @param xAudio2 オーディオ処理に使用するIXAudio2インターフェイスへのポインタ
   void Load(const std::wstring& filepath);

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
   std::vector<BYTE> audioData_;
   WAVEFORMATEX waveFormat_{};
   bool isLooping_ = false;
   XAUDIO2_BUFFER buffer_{};

   bool isPlaying_ = false;
   bool isPaused_ = false;
};
}