#pragma once

#include <xaudio2.h>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace GameEngine {
/// @brief すべての再生ボイスで共有するデコード済みPCMアセット。
class SoundClip final {
public:
   /// @brief Media FoundationでWAVまたはMP3をデコードする。失敗時は例外を送出する。
   /// @param path 非ASCII文字を含むネイティブファイルシステムパス。
   /// @return 完全なWAVEFORMATEXブロックを持つ変更不可のクリップ。
   static std::shared_ptr<const SoundClip> Decode(const std::wstring& path);
#ifdef MYPROJECT_NON_RELEASE
   /// @brief テスト用に、Media Foundationを使わず検証済みの小さなPCMフィクスチャを作成する。
   static std::shared_ptr<const SoundClip> CreateForTesting(const WAVEFORMATEX& format, std::vector<BYTE> pcm);
#endif

   /// @brief XAudio2のソースボイス作成に使用するフォーマットブロック。
   const WAVEFORMATEX* GetFormat() const { return reinterpret_cast<const WAVEFORMATEX*>(formatWords_.data()); }
   /// @brief いずれかのボイスが参照している間保持されるPCMデータ。
   const std::vector<BYTE>& GetPcm() const { return pcm_; }

private:
   std::vector<uint64_t> formatWords_;
   std::vector<BYTE> pcm_;
};
}
