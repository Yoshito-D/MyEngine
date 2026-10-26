#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace GameEngine {
/// @brief ビルド時の差分更新と、起動時の不足分生成を区別する。
enum class TextureCacheUpdateMode {
   ChangedAndMissing, ///< 元画像とDDSの内容を検証し、変更・欠落・破損分を生成する。
   MissingOnly,       ///< 既存DDSを再利用し、未生成・欠落分だけを生成する。
};

/// @brief DDS準備の結果。ビルド出力と通常起動のエラー処理で共有する。
struct TextureCacheResult {
   bool succeeded = false;    ///< すべての画像を準備できたか。
   std::size_t generated = 0; ///< 今回生成した画像数。
   std::size_t reused = 0;    ///< 既存DDSを再利用した画像数。
   double milliseconds = 0.0; ///< 準備にかかった時間。
   std::string error;         ///< 失敗した画像のパスと原因。
};

/// @brief Resources内のDDSとカタログを準備する。
/// @param resourcesRoot 元画像と生成物を管理するResourcesルート。
/// @param mode ビルド時はChangedAndMissing、通常起動時はMissingOnly。
/// @return 成否・生成件数・再利用件数と、失敗時の原因。
/// @note COM初期化済みのスレッドで、TextureManagerの初期化より前に呼び出す。
TextureCacheResult PrepareTextureCache(const std::filesystem::path& resourcesRoot = "Resources",
   TextureCacheUpdateMode mode = TextureCacheUpdateMode::MissingOnly);
}
