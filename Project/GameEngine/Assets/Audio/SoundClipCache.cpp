#include "GameEngine/pch.h"
#include "GameEngine/Assets/Audio/SoundClipCache.h"
#include "GameEngine/Assets/ResourceAssetPath.h"
#include <array>
#include <bcrypt.h>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>

#pragma comment(lib, "bcrypt.lib")

namespace GameEngine {
namespace {
// デコード規約や保存形式を変更した場合は、識別子の末尾の版番号も更新する。
constexpr std::array<char, 8> kCacheMagic = {'M', 'E', 'P', 'C', 'M', '0', '0', '1'};
constexpr std::uint64_t kCacheHeaderSize = kCacheMagic.size() + sizeof(std::uint64_t) * 4 + 32;

std::string NormalizeAudioId(std::string id) {
   // Windowsのパスは大文字小文字を区別しないため、UTF-8のバイト列を壊さないようASCIIだけを畳み込む。
   std::transform(id.begin(), id.end(), id.begin(), [](unsigned char character) {
      return character >= 'A' && character <= 'Z' ? static_cast<char>(character + ('a' - 'A'))
                                                  : static_cast<char>(character);
   });
   return id;
}

std::array<BYTE, 32> HashPcm(std::span<const BYTE> format, std::span<const BYTE> pcm) {
   BCRYPT_ALG_HANDLE algorithm = nullptr;
   BCRYPT_HASH_HANDLE hash = nullptr;

   if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
      throw std::runtime_error("Cannot open audio cache hash provider");
   }

   struct Guard {
      BCRYPT_ALG_HANDLE algorithm;
      BCRYPT_HASH_HANDLE& hash;

      ~Guard() {
         if (hash) {
            BCryptDestroyHash(hash);
         }

         BCryptCloseAlgorithmProvider(algorithm, 0);
      }
   } guard{algorithm, hash};

   if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0 ||
       BCryptHashData(hash, const_cast<BYTE*>(format.data()), static_cast<ULONG>(format.size()), 0) < 0 ||
       BCryptHashData(hash, const_cast<BYTE*>(pcm.data()), static_cast<ULONG>(pcm.size()), 0) < 0) {
      throw std::runtime_error("Cannot hash audio cache");
   }

   std::array<BYTE, 32> digest{};

   if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) {
      throw std::runtime_error("Cannot finish audio cache hash");
   }

   return digest;
}
}

std::filesystem::path SoundClipCache::GetDiskCachePath(
   const std::string& assetId, const std::filesystem::path& resourcesRoot) {
   if (ResourceAssetIdPath(assetId).empty()) {
      return {};
   }

   const auto id = NormalizeAudioId(assetId);
   const auto extension = ResourceAssetIdPath(id).extension();

   if (extension != ".wav" && extension != ".mp3") {
      return {};
   }

   for (const auto* root : kAudioRoots) {
      const auto prefix = std::string(root) + "/";

      if (id.starts_with(prefix)) {
         const auto relative = id.substr(prefix.size());

         if (relative.starts_with("cooked/")) {
            return {};
         }

         auto path = resourcesRoot / root / kAudioCookedDirectory / ResourceAssetIdPath(relative);
         path += L".pcmcache";
         return path;
      }
   }

   return {};
}

bool SoundClipCache::PrepareDiskCache(const std::string& assetId, const std::filesystem::path& resourcesRoot) {
   const auto source = ResolveResourceAssetPath(assetId, resourcesRoot);
   const auto cache = GetDiskCachePath(assetId, resourcesRoot);

   if (source.empty() || cache.empty()) {
      throw std::runtime_error("Invalid or missing audio source: " + assetId);
   }

   const auto sourceSize = std::filesystem::file_size(source);
   const auto modified = std::filesystem::last_write_time(source);

   try {
      if (ReadDiskCache(cache, sourceSize, modified)) {
         return false;
      }
   } catch (const std::exception&) {
      // 破損したキャッシュも元音源から再生成し、配布物へ不完全なPCMを含めない。
   }

   const auto clip = SoundClip::Decode(source.wstring());

   if (std::filesystem::file_size(source) != sourceSize || std::filesystem::last_write_time(source) != modified) {
      throw std::runtime_error("Audio source changed during cache generation: " + assetId);
   }

   WriteDiskCache(cache, *clip, sourceSize, modified);
   return true;
}

std::shared_ptr<const SoundClip> SoundClipCache::ReadDiskCache(const std::filesystem::path& path,
   std::optional<std::uint64_t> sourceSize, std::optional<std::filesystem::file_time_type> modified) {
   std::ifstream input(path, std::ios::binary | std::ios::ate);

   if (!input) {
      return nullptr;
   }

   const auto fileSize = input.tellg();
   input.seekg(0);
   std::array<char, 8> magic{};
   std::array<std::uint64_t, 4> metadata{};
   std::array<BYTE, 32> digest{};
   input.read(magic.data(), magic.size());
   input.read(reinterpret_cast<char*>(metadata.data()), sizeof(metadata));
   input.read(reinterpret_cast<char*>(digest.data()), digest.size());

   if (!input || magic != kCacheMagic) {
      throw std::runtime_error("Invalid audio cache header");
   }

   const auto& [cachedSourceSize, cachedSourceTime, formatSize, pcmSize] = metadata;

   if ((sourceSize && cachedSourceSize != *sourceSize) ||
       (modified && cachedSourceTime != static_cast<std::uint64_t>(modified->time_since_epoch().count()))) {
      return nullptr;
   }

   // 破損したサイズをそのままメモリ確保へ使わず、実ファイル長と再生形式の上限を先に検証する。
   if (formatSize < sizeof(WAVEFORMATEX) || formatSize > sizeof(WAVEFORMATEXTENSIBLE) || pcmSize == 0 ||
       pcmSize > std::numeric_limits<UINT32>::max() ||
       fileSize != static_cast<std::streamoff>(kCacheHeaderSize + formatSize + pcmSize)) {
      throw std::runtime_error("Invalid audio cache size");
   }

   std::vector<BYTE> formatBytes(static_cast<size_t>(formatSize));
   input.read(reinterpret_cast<char*>(formatBytes.data()), static_cast<std::streamsize>(formatSize));
   std::vector<BYTE> pcm(static_cast<size_t>(pcmSize));
   input.read(reinterpret_cast<char*>(pcm.data()), static_cast<std::streamsize>(pcmSize));

   if (!input || HashPcm(formatBytes, pcm) != digest) {
      throw std::runtime_error("Audio cache checksum mismatch");
   }

   return SoundClip::CreateFromPcm(formatBytes, std::move(pcm));
}

void SoundClipCache::WriteDiskCache(const std::filesystem::path& path, const SoundClip& clip, std::uint64_t sourceSize,
   std::filesystem::file_time_type modified) {
   const auto formatSize = sizeof(WAVEFORMATEX) + clip.GetFormat()->cbSize;
   const auto formatBytes = std::span(reinterpret_cast<const BYTE*>(clip.GetFormat()), formatSize);
   const auto digest = HashPcm(formatBytes, clip.GetPcm());
   const std::array<std::uint64_t, 4> metadata = {
      sourceSize, static_cast<std::uint64_t>(modified.time_since_epoch().count()), formatSize, clip.GetPcm().size()};
   std::filesystem::create_directories(path.parent_path());
   auto temporary = path;
   temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";

   // 同時起動するプロセスの一時ファイルを分け、失敗時にも未完成のPCMを残さない。
   struct Guard {
      const std::filesystem::path& path;

      ~Guard() {
         std::error_code error;
         std::filesystem::remove(path, error);
      }
   } guard{temporary};

   std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
   output.exceptions(std::ios::failbit | std::ios::badbit);
   output.write(kCacheMagic.data(), kCacheMagic.size());
   output.write(reinterpret_cast<const char*>(metadata.data()), sizeof(metadata));
   output.write(reinterpret_cast<const char*>(digest.data()), digest.size());
   output.write(reinterpret_cast<const char*>(formatBytes.data()), static_cast<std::streamsize>(formatSize));
   output.write(
      reinterpret_cast<const char*>(clip.GetPcm().data()), static_cast<std::streamsize>(clip.GetPcm().size()));
   output.close();

   if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      throw std::runtime_error("Cannot publish audio cache");
   }
}

std::shared_ptr<const SoundClip> SoundClipCache::GetClip(const std::string& assetId) {
   const auto cachePath = GetDiskCachePath(assetId);

   if (cachePath.empty()) {
      return nullptr;
   }

   const auto id = NormalizeAudioId(assetId);

   if (const auto found = clips_.find(id); found != clips_.end()) {
      return found->second;
   }

   const auto path = ResolveResourceAssetPath(assetId);
   const auto cacheId = ResourcePathToUtf8(cachePath.lexically_relative("Resources"));
   const auto resolvedCache = ResolveResourceAssetPath(cacheId);
   const auto& stampPath = path.empty() ? resolvedCache : path;
   std::error_code error;
   const auto modified =
      stampPath.empty() ? std::filesystem::file_time_type{} : std::filesystem::last_write_time(stampPath, error);
   const std::optional<std::filesystem::file_time_type> stamp =
      stampPath.empty() || error ? std::nullopt : std::optional(modified);

   if (const auto failed = failedClips_.find(id); failed != failedClips_.end() && failed->second == stamp) {
      return nullptr;
   }

   try {
      std::optional<std::uint64_t> sourceSize;

      if (!path.empty()) {
         const auto size = std::filesystem::file_size(path, error);

         if (!error) {
            sourceSize = size;
         }
      }

      const bool canCache = !path.empty() && stamp && sourceSize;
      std::shared_ptr<const SoundClip> clip;

      if (!resolvedCache.empty() && (path.empty() || canCache)) {
         try {
            clip = ReadDiskCache(resolvedCache, sourceSize, path.empty() ? std::nullopt : stamp);
         } catch (const std::exception& exception) {
            Logger::Warning("Audio cache will be rebuilt: " + id + " (" + exception.what() + ")");
         }
      }

      if (clip) {
         Logger::Info("Audio cache reused: " + id);
      } else {
         if (path.empty()) {
            throw std::runtime_error("Cooked audio is missing or invalid; rebuild audio caches");
         }

         clip = SoundClip::Decode(path.wstring());

         if (canCache) {
            try {
               // デコード中に元音源が更新された場合、その結果を古い更新情報では保存しない。
               if (std::filesystem::last_write_time(path) == *stamp &&
                   std::filesystem::file_size(path) == *sourceSize) {
                  WriteDiskCache(cachePath, *clip, *sourceSize, *stamp);
                  Logger::Info("Audio cache generated: " + id);
               }
            } catch (const std::exception& exception) {
               Logger::Warning("Audio cache save failed: " + id + " (" + exception.what() + ")");
            }
         }
      }

      clips_.emplace(id, clip);
      failedClips_.erase(id);
      return clip;
   } catch (const std::exception& exception) {
      failedClips_[id] = stamp;
      Logger::Warning("Audio asset unavailable: " + id + " (" + exception.what() + ")");
      return nullptr;
   }
}

} // namespace GameEngine
