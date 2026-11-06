#pragma once

#include "GameEngine/Audio/SoundClip.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace GameEngine {

/// @brief 元音源とPCMキャッシュを管理するResources相対のルート。
inline constexpr std::array<const char*, 2> kAudioRoots = {"engine/sounds", "game/sounds"};
/// @brief 各sounds内で全ビルド構成が共有するPCMキャッシュのフォルダー名。
inline constexpr const char* kAudioCookedDirectory = "Cooked";

/// @brief デコード済みPCMをメモリとディスクで再利用し、再生デバイスやボイスを所有しないキャッシュ。
class SoundClipCache final {
public:
   /// @brief 必要な音声だけを読み込み、同じアセットのPCMを共有する。
   /// @param assetId engine/soundsまたはgame/sounds内の元音源を指すResources相対の正規UTF-8アセットID。
   /// @return 共有クリップ。無効なID、欠落、デコード失敗時はnullptr。
   /// @details sounds/Cookedのキャッシュを利用し、元音源がある場合だけ更新を検証して必要時に再生成する。
   /// 元音源を含まない配布先でもキャッシュだけで読み込める。元音源のデコードにはMedia Foundationの初期化が必要。
   /// 読込失敗は元音源または配布キャッシュの更新時刻が変わるまで繰り返さず、欠落時は作成後に再試行する。
   std::shared_ptr<const SoundClip> GetClip(const std::string& assetId);

   /// @brief 元音源のIDを、同じsounds内のCooked配下に対応するPCMキャッシュパスへ変換する。
   /// @param assetId engine/soundsまたはgame/sounds内のWAV・MP3の正規アセットID。
   /// @param resourcesRoot 元音源とキャッシュを管理するResourcesルート。
   /// @return キャッシュのパス。不正なIDやCooked内の音源では空パス。
   static std::filesystem::path GetDiskCachePath(
      const std::string& assetId, const std::filesystem::path& resourcesRoot = "Resources");

   /// @brief ビルド用に1音源のキャッシュを検証し、更新・欠落・破損時だけ生成する。
   /// @param assetId 元音源を指すResources相対の正規UTF-8アセットID。
   /// @param resourcesRoot 元音源とキャッシュを管理するResourcesルート。
   /// @return 今回生成した場合はtrue、既存キャッシュを再利用した場合はfalse。
   /// @throws std::exception 元音源の読込、デコード、キャッシュ保存に失敗した場合。
   /// @note COMとMedia Foundationを初期化したスレッドで呼び出す。PCMをメモリキャッシュへ保持しない。
   static bool PrepareDiskCache(const std::string& assetId, const std::filesystem::path& resourcesRoot = "Resources");

private:
   /// @brief PCMを復元し、元音源がある場合だけ更新を検証する。欠落・更新時はnullptr、不正時は例外を送出する。
   static std::shared_ptr<const SoundClip> ReadDiskCache(const std::filesystem::path& path,
      std::optional<std::uint64_t> sourceSize, std::optional<std::filesystem::file_time_type> modified);

   /// @brief 形式情報とPCMを一時ファイルへ保存し、正常終了後にキャッシュを置き換える。
   static void WriteDiskCache(const std::filesystem::path& path, const SoundClip& clip, std::uint64_t sourceSize,
      std::filesystem::file_time_type modified);

   /// @brief 名前またはアセットIDに対応するクリップの一覧。
   std::unordered_map<std::string, std::shared_ptr<const SoundClip>> clips_;
   /// @brief 読み込み失敗時のファイル更新時刻。変更されるまで同じ失敗を繰り返さない。
   std::unordered_map<std::string, std::optional<std::filesystem::file_time_type>> failedClips_;
};

} // namespace GameEngine
