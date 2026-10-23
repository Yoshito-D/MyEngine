#include "GameEngine/pch.h"
#include "GameEngine/Assets/Audio/SoundClipCache.h"
#include "GameEngine/Assets/ResourceAssetPath.h"
#include <stdexcept>

namespace GameEngine {

std::shared_ptr<const SoundClip> SoundClipCache::GetClip(const std::string& assetId) {
   if (ResourceAssetIdPath(assetId).empty()) {
      return nullptr;
   }

   std::string id = assetId;

   // Windowsのパスは大文字小文字を区別しないため、UTF-8のバイト列を壊さないようASCIIだけを畳み込む。
   std::transform(id.begin(), id.end(), id.begin(), [](unsigned char character) {
      return character >= 'A' && character <= 'Z' ? static_cast<char>(character + ('a' - 'A'))
                                                  : static_cast<char>(character);
   });

   if (const auto found = clips_.find(id); found != clips_.end()) {
      return found->second;
   }

   const auto path = ResolveResourceAssetPath(assetId);
   std::error_code error;
   const auto modified =
      path.empty() ? std::filesystem::file_time_type{} : std::filesystem::last_write_time(path, error);
   const std::optional<std::filesystem::file_time_type> stamp =
      path.empty() || error ? std::nullopt : std::optional(modified);

   if (const auto failed = failedClips_.find(id); failed != failedClips_.end() && failed->second == stamp) {
      return nullptr;
   }

   try {
      if (path.empty()) {
         throw std::runtime_error("Audio asset is missing or outside resources");
      }

      auto clip = SoundClip::Decode(path.wstring());
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
