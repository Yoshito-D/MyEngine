#pragma once

#include "GameEngine/Audio/SoundClip.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace GameEngine {

/// @brief デコード済みの音声データを共有し、再生デバイスやボイスを所有しないキャッシュ。
class SoundClipCache final {
public:
   /// @brief Media Foundationの初期化後に音声を読み込み、同じアセットのPCMを共有する。
   /// @param assetId resources相対の正規UTF-8アセットID。
   /// @return 共有クリップ。無効なID、欠落、デコード失敗時はnullptr。
   /// @details 失敗したファイルは更新時刻が変わるまで再デコードしない。欠落ファイルは作成後に再試行する。
   std::shared_ptr<const SoundClip> GetClip(const std::string& assetId);

private:
   std::unordered_map<std::string, std::shared_ptr<const SoundClip>> clips_;
   std::unordered_map<std::string, std::optional<std::filesystem::file_time_type>> failedClips_;
};

} // namespace GameEngine
