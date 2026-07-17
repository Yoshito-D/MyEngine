#include "pch.h"
#include "FontManager.h"
#include "FontFace.h"
#include "GlyphAtlas.h"
#include "MsdfFont.h"
#include "Core/UI/Text/TextLayout.h"
#include "Graphics/GraphicsDevice.h"
#include <cctype>

namespace GameEngine {

FontManager::~FontManager() {
   Clear();
}

bool FontManager::Initialize(GraphicsDevice* device) {
   if (!device) {
      Logger::Error("[FontManager] GraphicsDevice is null.");
      return false;
   }

   Clear();
   device_ = device;
   const FT_Error error = FT_Init_FreeType(&library_);
   if (error != FT_Err_Ok) {
      Logger::Error("[FontManager] FT_Init_FreeType failed: error=" + std::to_string(error));
      library_ = nullptr;
      return false;
   }

   FT_Int major = 0;
   FT_Int minor = 0;
   FT_Int patch = 0;
   FT_Library_Version(library_, &major, &minor, &patch);
   Logger::Info(
      "[FontManager] FreeType runtime " + std::to_string(major) + "." +
      std::to_string(minor) + "." + std::to_string(patch) +
      " (headers " + std::to_string(FREETYPE_MAJOR) + "." +
      std::to_string(FREETYPE_MINOR) + "." + std::to_string(FREETYPE_PATCH) + ")");
   return true;
}

bool FontManager::LoadFont(const std::string& fontId, const std::filesystem::path& filePath) {
   if (!library_ || fontId.empty()) {
      Logger::Error("[FontManager] Invalid font load request.");
      return false;
   }

   auto face = std::make_unique<FontFace>();
   if (!face->Load(library_, filePath)) {
      return false;
   }

   for (auto iterator = atlases_.begin(); iterator != atlases_.end();) {
      if (iterator->first.fontId == fontId) {
         iterator = atlases_.erase(iterator);
      } else {
         ++iterator;
      }
   }
   fonts_[fontId] = std::move(face);
   msdfFonts_.erase(fontId);
   ++revision_;
   Logger::Info("[FontManager] Loaded font: " + fontId + " <- " + filePath.generic_string());
   return true;
}

bool FontManager::LoadMsdfFont(const std::string& fontId, const std::filesystem::path& jsonPath) {
   if (!device_ || fontId.empty()) {
      Logger::Error("[FontManager] Invalid MSDF font load request.");
      return false;
   }

   auto font = std::make_unique<MsdfFont>();
   if (!font->Load(device_, jsonPath)) {
      return false;
   }

   for (auto iterator = atlases_.begin(); iterator != atlases_.end();) {
      if (iterator->first.fontId == fontId) {
         iterator = atlases_.erase(iterator);
      } else {
         ++iterator;
      }
   }
   fonts_.erase(fontId);
   msdfFonts_[fontId] = std::move(font);
   ++revision_;
   Logger::Info("[FontManager] Loaded MSDF font: " + fontId + " <- " + jsonPath.generic_string());
   return true;
}

size_t FontManager::LoadFontsFromDirectory(const std::filesystem::path& directory) {
   std::error_code error;
   if (!std::filesystem::is_directory(directory, error)) {
      return 0;
   }

   size_t loadedCount = 0;
   std::filesystem::recursive_directory_iterator iterator(
      directory,
      std::filesystem::directory_options::skip_permission_denied,
      error);
   const std::filesystem::recursive_directory_iterator end;
   while (iterator != end) {
      if (error) {
         error.clear();
         iterator.increment(error);
         continue;
      }

      const std::filesystem::directory_entry& entry = *iterator;
      if (entry.is_regular_file(error)) {
         std::string extension = entry.path().extension().string();
         std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
         });
         if (extension == ".ttf" || extension == ".otf" || extension == ".ttc") {
            std::filesystem::path relativePath = std::filesystem::relative(entry.path(), directory, error);
            if (error) {
               error.clear();
               relativePath = entry.path().filename();
            }
            relativePath.replace_extension();
            if (LoadFont(relativePath.generic_string(), entry.path())) {
               ++loadedCount;
            }
         }
      }
      error.clear();
      iterator.increment(error);
   }
   return loadedCount;
}

size_t FontManager::LoadMsdfFontsFromDirectory(const std::filesystem::path& directory) {
   std::error_code error;
   if (!std::filesystem::is_directory(directory, error)) {
      return 0;
   }

   size_t loadedCount = 0;
   std::filesystem::recursive_directory_iterator iterator(
      directory,
      std::filesystem::directory_options::skip_permission_denied,
      error);
   const std::filesystem::recursive_directory_iterator end;
   while (iterator != end) {
      if (error) {
         error.clear();
         iterator.increment(error);
         continue;
      }

      const std::filesystem::directory_entry& entry = *iterator;
      if (entry.is_regular_file(error)) {
         std::string extension = entry.path().extension().string();
         std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
         });
         if (extension == ".json") {
            std::filesystem::path relativePath = std::filesystem::relative(entry.path(), directory, error);
            if (error) {
               error.clear();
               relativePath = entry.path().filename();
            }
            relativePath.replace_extension();
            if (LoadMsdfFont(relativePath.generic_string(), entry.path())) {
               ++loadedCount;
            }
         }
      }
      error.clear();
      iterator.increment(error);
   }
   return loadedCount;
}

bool FontManager::HasFont(const std::string& fontId) const {
   return fonts_.contains(fontId) || msdfFonts_.contains(fontId);
}

std::vector<std::string> FontManager::GetFontIds() const {
   std::vector<std::string> ids;
   ids.reserve(fonts_.size() + msdfFonts_.size());
   for (const auto& [fontId, face] : fonts_) {
      (void)face;
      ids.push_back(fontId);
   }
   for (const auto& [fontId, font] : msdfFonts_) {
      (void)font;
      if (!fonts_.contains(fontId)) {
         ids.push_back(fontId);
      }
   }
   std::sort(ids.begin(), ids.end());
   return ids;
}

TextLayoutResult FontManager::LayoutText(std::string_view text, const TextStyle& style) {
   return TextLayout::Build(*this, text, style);
}

const GlyphInfo* FontManager::GetOrCreateGlyph(const std::string& fontId, uint32_t pixelSize, char32_t codePoint) {
   if (const auto msdfIterator = msdfFonts_.find(fontId); msdfIterator != msdfFonts_.end()) {
      return msdfIterator->second->GetGlyph(pixelSize, codePoint);
   }

   FontFace* face = FindFont(fontId);
   GlyphAtlas* atlas = GetOrCreateAtlas(fontId, pixelSize);
   if (!face || !atlas) {
      return nullptr;
   }

   char32_t resolvedCodePoint = codePoint;
   if (!face->HasGlyph(resolvedCodePoint)) {
      if (face->HasGlyph(0xFFFD)) {
         resolvedCodePoint = 0xFFFD;
      } else if (face->HasGlyph(U'?')) {
         resolvedCodePoint = U'?';
      } else {
         return nullptr;
      }
   }

   if (const GlyphInfo* cached = atlas->FindGlyph(resolvedCodePoint)) {
      return cached;
   }

   RasterizedGlyph rasterized{};
   if (!face->RasterizeGlyph(resolvedCodePoint, pixelSize, rasterized)) {
      return nullptr;
   }
   return atlas->AddGlyph(resolvedCodePoint, rasterized);
}

FontMetrics FontManager::GetMetrics(const std::string& fontId, uint32_t pixelSize) {
   if (const auto iterator = msdfFonts_.find(fontId); iterator != msdfFonts_.end()) {
      return iterator->second->GetMetrics(pixelSize);
   }
   FontFace* face = FindFont(fontId);
   return face ? face->GetMetrics(pixelSize) : FontMetrics{};
}

float FontManager::GetKerning(const std::string& fontId, uint32_t leftGlyph, uint32_t rightGlyph, uint32_t pixelSize) {
   if (const auto iterator = msdfFonts_.find(fontId); iterator != msdfFonts_.end()) {
      return iterator->second->GetKerning(leftGlyph, rightGlyph, pixelSize);
   }
   FontFace* face = FindFont(fontId);
   return face ? face->GetKerning(leftGlyph, rightGlyph, pixelSize) : 0.0f;
}

void FontManager::FlushPendingUploads() {
   for (auto& [key, atlas] : atlases_) {
      (void)key;
      atlas->FlushPendingUploads();
   }
}

void FontManager::ReleaseIntermediateResources() {
   for (auto& [key, atlas] : atlases_) {
      (void)key;
      atlas->ReleaseIntermediateResources();
   }
   for (auto& [fontId, font] : msdfFonts_) {
      (void)fontId;
      font->ReleaseIntermediateResources();
   }
}

void FontManager::Clear() {
   atlases_.clear();
   msdfFonts_.clear();
   fonts_.clear();
   if (library_) {
      FT_Done_FreeType(library_);
      library_ = nullptr;
   }
   device_ = nullptr;
   ++revision_;
}

size_t FontManager::AtlasKeyHash::operator()(const AtlasKey& key) const {
   const size_t fontHash = std::hash<std::string>{}(key.fontId);
   const size_t sizeHash = std::hash<uint32_t>{}(key.pixelSize);
   return fontHash ^ (sizeHash + 0x9e3779b9u + (fontHash << 6u) + (fontHash >> 2u));
}

FontFace* FontManager::FindFont(const std::string& fontId) const {
   const auto iterator = fonts_.find(fontId);
   return iterator != fonts_.end() ? iterator->second.get() : nullptr;
}

GlyphAtlas* FontManager::GetOrCreateAtlas(const std::string& fontId, uint32_t pixelSize) {
   if (!device_ || pixelSize == 0 || !HasFont(fontId)) {
      return nullptr;
   }

   AtlasKey key{ fontId, pixelSize };
   const auto iterator = atlases_.find(key);
   if (iterator != atlases_.end()) {
      return iterator->second.get();
   }

   auto atlas = std::make_unique<GlyphAtlas>(device_);
   GlyphAtlas* result = atlas.get();
   atlases_.emplace(std::move(key), std::move(atlas));
   return result;
}

} // namespace GameEngine
