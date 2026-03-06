#pragma once
#include <xaudio2.h>
#include <wrl.h>

using namespace Microsoft::WRL;

namespace GameEngine {
/// @brief オーディオクラス
class Audio {
public:
   /// @brief オーディオの初期化
   void Initialize();

   /// @brief オーディオの終了処理
   void Finalize();

   /// @brief XAudio2インターフェースを取得
   IXAudio2* GetXAudio2() const { return xAudio2_.Get(); }

private:
   ComPtr<IXAudio2> xAudio2_ = nullptr;
   IXAudio2MasteringVoice* masteringVoice_ = nullptr;
};
}