#pragma once
#include <filesystem>
#include <string>

namespace GameEngine {
/// @brief リソースパスをスラッシュ区切りのUTF-8文字列へ変換する。
inline std::string ResourcePathToUtf8(const std::filesystem::path& path) {
   const auto utf8 = path.generic_u8string();
   return { utf8.begin(), utf8.end() };
}

/// @brief 正規のresources相対IDをUTF-8パスへ変換し、無効なIDでは空パスを返す。
/// @param assetId 正規化済みのスラッシュ区切りID。絶対パスや短縮名への推測は行わない。
inline std::filesystem::path ResourceAssetIdPath(const std::string& assetId) {
   if (assetId.empty() || assetId.size() > 4096 || assetId.find_first_of("<>:\"|?*") != std::string::npos) return {};
   for (unsigned char character : assetId) if (character < 32) return {};
   try {
      const std::filesystem::path relative(std::u8string(assetId.begin(), assetId.end()));
      if (relative.has_root_path() || ResourcePathToUtf8(relative.lexically_normal()) != assetId) return {};
      for (const auto& part : relative) if (part == ".." || part == ".") return {};
      return relative;
   } catch (const std::system_error&) {
      return {};
   }
}

/// @brief 正規のアセットIDから、実在するリソースルート内ファイルだけを解決する。
/// @param assetId resourcesRootからの相対ID。
/// @param resourcesRoot リソースのルートディレクトリ。
/// @return 読込可能なファイルの絶対パス。無効・欠落・ルート外の場合は空パス。
inline std::filesystem::path ResolveResourceAssetPath(const std::string& assetId,
   const std::filesystem::path& resourcesRoot = "resources") {
   const auto relative = ResourceAssetIdPath(assetId);
   if (relative.empty()) return {};
   std::error_code error;
   const auto root = std::filesystem::weakly_canonical(resourcesRoot, error);
   if (error) return {};
   const auto candidate = std::filesystem::weakly_canonical(root / relative, error);
   if (error) return {};
   // Junctionやシンボリックリンクでも、実際の読込先がルートを逸脱しないことを確認する。
   auto candidatePart = candidate.begin();
   for (const auto& rootPart : root) {
      if (candidatePart == candidate.end() || *candidatePart != rootPart) return {};
      ++candidatePart;
   }
   if (candidatePart == candidate.end() || !std::filesystem::is_regular_file(candidate, error) || error) return {};
   return candidate;
}
}
