#pragma once

#include <xaudio2.h>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <span>

namespace GameEngine {

/// @brief すべての再生ボイスで共有するデコード済みPCMアセット。
class SoundClip final {
public:
   /// @brief Media FoundationでWAVまたはMP3をデコードする。失敗時は例外を送出する。
   /// @param path 非ASCII文字を含むネイティブファイルシステムパス。
   /// @return 完全なWAVEFORMATEXブロックを持つ変更不可のクリップ。
   static std::shared_ptr<const SoundClip> Decode(const std::wstring& path);

   /// @brief PCMと形式情報を検証し、変更不可のクリップを生成する。無効な入力では例外を送出する。
   /// @param format WAVEFORMATEXと拡張情報を含む完全な形式ブロック。
   /// @param pcm 全チャンネルを含むサンプル単位のPCM。所有権を受け取る。
   /// @return 検証済みの形式とPCMを所有するクリップ。
   static std::shared_ptr<const SoundClip> CreateFromPcm(std::span<const BYTE> format, std::vector<BYTE> pcm);
#ifdef MYPROJECT_NON_RELEASE

   /// @brief テスト用に、Media Foundationを使わず検証済みの小さなPCMフィクスチャを作成する。
   static std::shared_ptr<const SoundClip> CreateForTesting(const WAVEFORMATEX& format, std::vector<BYTE> pcm);
#endif

   /// @brief XAudio2のソースボイス作成に使用するフォーマットブロック。
   const WAVEFORMATEX* GetFormat() const { return reinterpret_cast<const WAVEFORMATEX*>(formatWords_.data()); }

   /// @brief いずれかのボイスが参照している間保持されるPCMデータ。
   const std::vector<BYTE>& GetPcm() const { return pcm_; }

private:
   SoundClip() = default;

   std::vector<uint64_t> formatWords_; ///< WAVEFORMATEXを十分な配置境界で保持する所有ストレージ。
   std::vector<BYTE> pcm_;             ///< いずれかのボイスが参照している間保持されるPCMデータ。
};
}
