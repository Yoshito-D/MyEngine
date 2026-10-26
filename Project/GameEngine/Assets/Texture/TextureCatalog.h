#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <string>
#include <dxgiformat.h>

namespace GameEngine {
/// @brief DDS生成規約。色空間・ミップ生成方法・カタログ形式を変更したら更新する。
inline constexpr std::int32_t kTextureCookVersion = 3;
/// @brief 通常テクスチャを管理するResources相対のルート。
inline constexpr std::array<const char*, 2> kTextureRoots = {"engine/textures", "game/textures"};

/// @brief 各textures内で全ビルド構成が共有するDDS・カタログのフォルダー名。
inline constexpr const char* kTextureCookedDirectory = "Cooked";
/// @brief 一般画像から事前生成するミップ段階数の上限。
inline constexpr std::size_t kTextureCookMipLevels = 4;

/// @brief GPUへ読み込まずに参照できる、ビルド済みテクスチャの情報。
struct TextureInfo {
   std::string name;                         ///< フォルダーと拡張子に依存しない画像名。
   std::filesystem::path sourcePath;         ///< エディタのファイル選択用Resources相対パス。
   std::filesystem::path ddsPath;            ///< Resources相対の生成DDSパス。
   std::uint32_t width = 0;                  ///< 最上位ミップの幅。
   std::uint32_t height = 0;                 ///< 最上位ミップの高さ。
   std::uint32_t mipLevels = 0;              ///< DDSに格納済みのミップ段階数。
   DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN; ///< 格納済みのピクセル形式。
   bool isCubemap = false;                   ///< 6面のキューブマップかどうか。
};
}
