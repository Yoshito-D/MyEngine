#include "GameEngine/pch.h"
#include "GameEngine/Assets/Texture/TextureCacheBuilder.h"
#include "Externals/DirectXTex/DirectXTex.h"
#include "GameEngine/Assets/Texture/TextureCatalog.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <bcrypt.h>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace GameEngine {
namespace {
using Json = nlohmann::json;

std::string Utf8(const std::filesystem::path& path) {
   const auto value = path.generic_u8string();
   return {value.begin(), value.end()};
}

void Check(HRESULT result, const char* operation) {
   if (FAILED(result)) {
      throw std::runtime_error(std::string(operation) + " failed (HRESULT=" + std::to_string(result) + ")");
   }
}

std::string HashFile(const std::filesystem::path& path) {
   BCRYPT_ALG_HANDLE algorithm = nullptr;
   BCRYPT_HASH_HANDLE hash = nullptr;

   if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
      throw std::runtime_error("Cannot open SHA-256 provider");
   }

   // ローカルハンドルは、入力ファイルの読込失敗時にも必ず解放する。
   struct Guard {
      BCRYPT_ALG_HANDLE& algorithm;
      BCRYPT_HASH_HANDLE& hash;

      ~Guard() {
         if (hash) {
            BCryptDestroyHash(hash);
         }

         BCryptCloseAlgorithmProvider(algorithm, 0);
      }
   } guard{algorithm, hash};

   if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0) {
      throw std::runtime_error("Cannot create SHA-256 hash");
   }

   std::ifstream input(path, std::ios::binary);

   if (!input) {
      throw std::runtime_error("Cannot read image: " + Utf8(path));
   }

   std::array<unsigned char, 65536> buffer{};

   while (input.read(reinterpret_cast<char*>(buffer.data()), buffer.size()) || input.gcount()) {
      if (BCryptHashData(hash, buffer.data(), static_cast<ULONG>(input.gcount()), 0) < 0) {
         throw std::runtime_error("Cannot hash image");
      }
   }

   if (!input.eof()) {
      throw std::runtime_error("Image read failed: " + Utf8(path));
   }

   std::array<unsigned char, 32> digest{};

   if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) {
      throw std::runtime_error("Cannot finish image hash");
   }

   constexpr char hex[] = "0123456789abcdef";
   std::string result;

   for (const auto byte : digest) {
      result += hex[byte >> 4];
      result += hex[byte & 15];
   }

   return result;
}

std::string Lower(std::string text) {
   std::transform(text.begin(), text.end(), text.begin(),
      [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
   return text;
}

void ReplaceFile(const std::filesystem::path& temporary, const std::filesystem::path& destination) {
   if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      throw std::runtime_error("Cannot publish generated file: " + Utf8(destination));
   }
}

Json Cook(const std::filesystem::path& source, const std::filesystem::path& resources,
   const std::filesystem::path& output, const std::string& name, const std::string& hash) {
   DirectX::ScratchImage image;

   if (Lower(Utf8(source.extension())) == ".dds") {
      Check(DirectX::LoadFromDDSFile(source.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image), "DDS decode");
   } else {
      Check(DirectX::LoadFromWICFile(source.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image), "WIC decode");
      size_t levels = 1;

      for (size_t dimension = std::max(image.GetMetadata().width, image.GetMetadata().height);
         dimension > 1 && levels < GameEngine::kTextureCookMipLevels; dimension >>= 1) {
         ++levels;
      }

      if (levels > 1) {
         DirectX::ScratchImage mipmaps;
         Check(DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
                  DirectX::TEX_FILTER_SRGB, levels, mipmaps),
            "Mip generation");
         image = std::move(mipmaps);
      }
   }

   const auto& metadata = image.GetMetadata();

   if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || !metadata.width || !metadata.height ||
       metadata.width > 16384 || metadata.height > 16384 || !metadata.mipLevels || metadata.mipLevels > 15 ||
       (metadata.IsCubemap() ? metadata.arraySize != 6 : metadata.arraySize != 1)) {
      throw std::runtime_error("Unsupported texture dimensions or array");
   }

   const auto relative = source.stem();
   const auto ddsName = std::filesystem::path(relative.wstring() + L".dds");
   const auto destination = output / ddsName;
   std::filesystem::create_directories(output);
   auto temporary = destination;
   temporary += L".tmp";
   Check(DirectX::SaveToDDSFile(
            image.GetImages(), image.GetImageCount(), metadata, DirectX::DDS_FLAGS_NONE, temporary.c_str()),
      "DDS save");
   ReplaceFile(temporary, destination);
   return {{"name", name}, {"sourcePath", Utf8(source.lexically_relative(resources))},
      {"ddsPath", Utf8(destination.lexically_relative(resources))}, {"sourceHash", hash},
      {"ddsHash", HashFile(destination)}, {"width", metadata.width}, {"height", metadata.height},
      {"mipLevels", metadata.mipLevels}, {"format", static_cast<std::uint32_t>(metadata.format)},
      {"isCubemap", metadata.IsCubemap()}};
}

TextureCacheResult BuildTextureCache(const std::filesystem::path& resources, TextureCacheUpdateMode mode) {
   if (!std::filesystem::is_directory(resources)) {
      throw std::runtime_error("Resources folder does not exist: " + Utf8(resources));
   }

   // 先に全ルートの入力を集め、engineとgameをまたぐ同名画像も変換前に検出する。
   const auto compareNames = [](const std::wstring& lhs, const std::wstring& rhs) {
      return CompareStringOrdinal(lhs.c_str(), -1, rhs.c_str(), -1, TRUE) == CSTR_LESS_THAN;
   };
   std::map<std::wstring, std::pair<std::filesystem::path, std::string>, decltype(compareNames)> sources(compareNames);
   std::map<std::string, Json> previous;
   std::map<std::string, Json> catalogs;
   std::map<std::string, std::pair<std::string, Json>> cached;

   for (const auto* rootId : kTextureRoots) {
      const auto textureRoot = resources / rootId;
      const auto output = textureRoot / kTextureCookedDirectory;
      const auto catalogPath = output / "catalog.json";
      Json oldCatalog;

      if (std::ifstream input(catalogPath); input) {
         oldCatalog = Json::parse(input, nullptr, false);
      }

      previous.emplace(rootId, oldCatalog);
      catalogs.emplace(rootId, Json{{"version", kTextureCookVersion}, {"textures", Json::array()}});

      if (oldCatalog.is_object() && oldCatalog.value("version", 0) == kTextureCookVersion &&
          oldCatalog.contains("textures") && oldCatalog["textures"].is_array()) {
         for (const auto& entry : oldCatalog["textures"]) {
            const auto name = entry.at("name").get<std::string>();
            const auto namePath = std::filesystem::path(std::u8string(name.begin(), name.end()));
            const auto expected = std::filesystem::path(rootId) / kTextureCookedDirectory /
                                  std::filesystem::path(namePath.wstring() + L".dds");

            if (name.empty() || namePath.has_parent_path() || namePath.has_root_path() ||
                entry.value("ddsPath", "") != Utf8(expected) ||
                !cached.emplace(name, std::make_pair(std::string(rootId), entry)).second) {
               throw std::runtime_error("Invalid or duplicate cooked texture catalog entry: " + name);
            }
         }
      }

      if (!std::filesystem::is_directory(textureRoot)) {
         continue;
      }

      for (auto iterator = std::filesystem::recursive_directory_iterator(textureRoot);
         iterator != std::filesystem::recursive_directory_iterator(); ++iterator) {
         if (iterator->is_symlink()) {
            iterator.disable_recursion_pending();
            continue;
         }

         const auto& source = iterator->path();

         if (iterator->is_directory()) {
            if (Lower(Utf8(source.lexically_relative(textureRoot))) == Lower(kTextureCookedDirectory)) {
               iterator.disable_recursion_pending();
            }

            continue;
         }

         const auto extension = Lower(Utf8(source.extension()));

         if (!iterator->is_regular_file() ||
             (extension != ".png" && extension != ".jpg" && extension != ".jpeg" && extension != ".dds")) {
            continue;
         }

         const auto [found, inserted] = sources.emplace(source.stem().wstring(), std::make_pair(source, rootId));

         if (!inserted) {
            throw std::runtime_error("Duplicate texture name '" + Utf8(source.stem()) +
                                     "': " + Utf8(found->second.first) + " and " + Utf8(source) +
                                     ". Give each image a unique name.");
         }
      }
   }

   size_t converted = 0;
   size_t reused = 0;
   std::set<std::wstring, decltype(compareNames)> expectedNames(compareNames);
   std::set<std::wstring, decltype(compareNames)> expectedDds(compareNames);

   for (const auto& [wideName, input] : sources) {
      (void)wideName;
      const auto& [source, rootId] = input;

      try {
         const auto name = Utf8(source.stem());
         const auto output = resources / rootId / kTextureCookedDirectory;
         const auto destination = output / std::filesystem::path(source.stem().wstring() + L".dds");
         const auto ddsId = Utf8(destination.lexically_relative(resources));
         std::string hash;

         if (mode == TextureCacheUpdateMode::ChangedAndMissing) {
            hash = HashFile(source);
         }

         expectedNames.insert(source.stem().wstring());
         expectedDds.insert(destination.lexically_relative(resources).generic_wstring());
         const auto found = cached.find(name);

         if (found != cached.end() && found->second.second.value("ddsPath", "") == ddsId &&
             std::filesystem::is_regular_file(destination) &&
             (mode == TextureCacheUpdateMode::MissingOnly ||
                (found->second.second.value("sourceHash", "") == hash &&
                   found->second.second.value("ddsHash", "") == HashFile(destination)))) {
            auto entry = found->second.second;
            // 同じtextures内のフォルダー移動では、画像名と生成DDSを維持する。
            entry["sourcePath"] = Utf8(source.lexically_relative(resources));
            catalogs.at(rootId)["textures"].push_back(std::move(entry));
            ++reused;
            continue;
         }

         if (hash.empty()) {
            hash = HashFile(source);
         }

         catalogs.at(rootId)["textures"].push_back(Cook(source, resources, output, name, hash));
         ++converted;
      } catch (const std::exception& error) {
         throw std::runtime_error(Utf8(source) + ": " + error.what());
      }
   }

   // 配布先には元画像がなくてもよい。起動時は既存DDSと名前の対応を保持する。
   if (mode == TextureCacheUpdateMode::MissingOnly) {
      for (const auto& [name, owner] : cached) {
         if (expectedNames.contains(std::filesystem::path(std::u8string(name.begin(), name.end())).wstring())) {
            continue;
         }

         const auto& [rootId, entry] = owner;
         const auto id = entry.at("ddsPath").get<std::string>();
         const auto relative = std::filesystem::path(std::u8string(id.begin(), id.end()));

         if (!std::filesystem::is_regular_file(resources / relative)) {
            throw std::runtime_error(
               "Missing DDS cannot be regenerated because its source image is unavailable: " + name);
         }

         catalogs.at(rootId)["textures"].push_back(entry);
         ++reused;
      }
   }

   // 両ルートの変換が成功してから、各カタログを一時ファイル経由で公開する。
   for (const auto& [rootId, catalog] : catalogs) {
      if (catalog == previous.at(rootId)) {
         continue;
      }

      const auto output = resources / rootId / kTextureCookedDirectory;
      std::filesystem::create_directories(output);
      const auto temporary = output / "catalog.json.tmp";
      std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
      stream << catalog.dump(3) << '\n';
      stream.close();

      if (!stream) {
         throw std::runtime_error("Cannot save catalog: " + Utf8(temporary));
      }
   }

   for (const auto& [rootId, catalog] : catalogs) {
      if (catalog != previous.at(rootId)) {
         const auto output = resources / rootId / kTextureCookedDirectory;
         ReplaceFile(output / "catalog.json.tmp", output / "catalog.json");
      }
   }

   if (mode == TextureCacheUpdateMode::ChangedAndMissing) {
      for (const auto& [name, owner] : cached) {
         (void)name;
         const auto id = owner.second.at("ddsPath").get<std::string>();

         if (expectedDds.contains(std::filesystem::path(std::u8string(id.begin(), id.end())).generic_wstring())) {
            continue;
         }

         const auto path = resources / std::filesystem::path(std::u8string(id.begin(), id.end()));

         // 上で所有ルートと単一のDDS名を検証した、前回の生成物だけを削除する。
         if (!std::filesystem::is_symlink(path) && std::filesystem::is_regular_file(path)) {
            std::filesystem::remove(path);
         }
      }
   }

   return {true, converted, reused, 0.0, {}};
}
} // namespace

TextureCacheResult PrepareTextureCache(const std::filesystem::path& resourcesRoot, TextureCacheUpdateMode mode) {
   const auto start = std::chrono::steady_clock::now();
   TextureCacheResult result;
   try {
      const auto resources = std::filesystem::absolute(resourcesRoot).lexically_normal();
      // 全構成で同じ生成物を更新するため、ビルド・起動プロセス間で生成を直列化する。
      HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\MyEngine.TextureCache");

      if (!mutex) {
         throw std::runtime_error("Cannot create texture cache generation mutex");
      }

      struct Guard {
         HANDLE mutex;
         bool acquired = false;

         ~Guard() {
            if (acquired) {
               ReleaseMutex(mutex);
            }

            CloseHandle(mutex);
         }
      } guard{mutex};

      const DWORD waitResult = WaitForSingleObject(mutex, INFINITE);

      if (waitResult != WAIT_OBJECT_0 && waitResult != WAIT_ABANDONED) {
         throw std::runtime_error("Cannot acquire texture cache generation mutex");
      }

      guard.acquired = true;
      result = BuildTextureCache(resources, mode);
   } catch (const std::exception& error) {
      result.error = error.what();
   }

   result.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
   return result;
}
}
