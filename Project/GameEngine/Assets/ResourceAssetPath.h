#pragma once

#include <filesystem>
#include <string>

namespace GameEngine {

/// @brief リソースパスをスラッシュ区切りのUTF-8文字列へ変換する。
std::string ResourcePathToUtf8(const std::filesystem::path& path);

/// @brief 正規のresources相対IDをUTF-8パスへ変換し、無効なIDでは空パスを返す。
/// @param assetId 正規化済みのスラッシュ区切りID。絶対パスや短縮名への推測は行わない。
std::filesystem::path ResourceAssetIdPath(const std::string& assetId);

/// @brief 正規のアセットIDから、実在するリソースルート内ファイルだけを解決する。
/// @param assetId resourcesRootからの相対ID。
/// @param resourcesRoot リソースのルートディレクトリ。
/// @return 読込可能なファイルの絶対パス。無効・欠落・ルート外の場合は空パス。
std::filesystem::path ResolveResourceAssetPath(
   const std::string& assetId, const std::filesystem::path& resourcesRoot = "resources");
}
