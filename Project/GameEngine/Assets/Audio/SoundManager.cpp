#include "GameEngine/pch.h"
#include "GameEngine/Assets/Audio/SoundManager.h"
#include "GameEngine/Audio/Audio.h"
#include <cassert>
#include <filesystem>
#include <algorithm>

namespace GameEngine {
namespace {
std::filesystem::path Utf8Path(const std::string& value) {
   return std::filesystem::path(std::u8string(value.begin(), value.end()));
}
}
void SoundManager::Initialize(Audio* audio) {
   assert(audio != nullptr);
   audio_ = audio;
}

void SoundManager::LoadSound(const std::string& filePath, const std::string& name) {
   // 既にロード済みの場合は何もしない
   if (sounds_.find(name) != sounds_.end()) {
	  return;
   }

   // 新しいサウンドを作成
   auto sound = std::make_unique<Sound>();

   const auto legacyPath = Utf8Path(filePath);
   const auto first = legacyPath.begin();
   if (first != legacyPath.end() && (*first == L"Resources" || *first == L"resources")) {
      auto clip = GetClip(filePath);
      if (!clip) throw std::runtime_error("Failed to load sound asset");
      sound->LoadClip(std::move(clip));
   } else {
      sound->Load(legacyPath.wstring());
   }

   // マップに追加
   sounds_[name] = std::move(sound);
}

Sound* SoundManager::GetSound(const std::string& name) {
   auto it = sounds_.find(name);
   if (it != sounds_.end()) {
	  return it->second.get();
   }
   return nullptr;
}

void SoundManager::Clear() {
   sounds_.clear();
   clips_.clear();
   failedClips_.clear();
}

std::string SoundManager::NormalizeAssetId(const std::string& assetId) {
   if (assetId.empty()) return {};
   try {
      auto path = Utf8Path(assetId).lexically_normal();
      if (path.is_absolute() || path.has_root_name()) return {};
      auto first = path.begin();
      if (first != path.end() && (*first == L"resources" || *first == L"Resources")) ++first;
      std::filesystem::path relative;
      for (; first != path.end(); ++first) {
         if (*first == L".." || *first == L".") return {};
         relative /= *first;
      }
      if (relative.empty()) return {};
      const auto normalized = relative.generic_u8string();
      std::string result(normalized.begin(), normalized.end());
      // Windowsのパスは大文字小文字を区別しないため、UTF-8のバイト列を壊さないようASCIIだけを畳み込む。
      std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
         return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : static_cast<char>(c);
      });
      return result;
   } catch (const std::filesystem::filesystem_error&) {
      return {};
   }
}

std::shared_ptr<const SoundClip> SoundManager::GetClip(const std::string& assetId) {
   const std::string id = NormalizeAssetId(assetId);
   if (id.empty()) return nullptr;
   if (auto found = clips_.find(id); found != clips_.end()) return found->second;
   const auto path = std::filesystem::path(L"Resources") / Utf8Path(id);
   std::error_code pathError;
   const auto modified = std::filesystem::last_write_time(path, pathError);
   const std::optional<std::filesystem::file_time_type> stamp = pathError ? std::nullopt : std::optional(modified);
   if (auto failed = failedClips_.find(id); failed != failedClips_.end() && failed->second == stamp)
      return nullptr;
   try {
      auto clip = SoundClip::Decode(path.wstring());
      clips_.emplace(id, clip);
      failedClips_.erase(id);
      return clip;
   } catch (const std::exception& error) {
      failedClips_[id] = stamp;
      Logger::Warning("Audio asset unavailable: " + id + " (" + error.what() + ")");
      return nullptr;
   }
}
}
