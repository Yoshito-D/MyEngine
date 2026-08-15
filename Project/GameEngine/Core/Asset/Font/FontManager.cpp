#include "pch.h"
#include "FontManager.h"
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
   return true;
}

bool FontManager::LoadMsdfFont(const std::string& fontId, const std::filesystem::path& jsonPath) {
   if (!device_ || fontId.empty()) {
      Logger::Error("[FontManager] Invalid MSDF font load request.");
      return false;
   }

   auto font = std::make_unique<MsdfFont>();
   // 新フォントを完全にロードしてから同じIDへ差し替え、失敗時は既存フォントを残す。
   if (!font->Load(device_, jsonPath)) {
      return false;
   }

   msdfFonts_[fontId] = std::move(font);
   // UIText側のレイアウトキャッシュへフォント差し替えを通知する世代番号を進める。
   ++revision_;
   Logger::Info("[FontManager] Loaded MSDF font: " + fontId + " <- " + jsonPath.generic_string());
   return true;
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
   // error_code版を使い、アクセス不能な項目を例外で全走査失敗にしない。
   while (iterator != end) {
      if (error) {
         // 読めない1ディレクトリで全フォントの自動検出を中断しない。
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
            // ディレクトリ相対パスをIDにして、同名フォントをサブフォルダーで共存させる。
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
   return msdfFonts_.contains(fontId);
}

std::vector<std::string> FontManager::GetFontIds() const {
   std::vector<std::string> ids;
   ids.reserve(msdfFonts_.size());
   for (const auto& [fontId, font] : msdfFonts_) {
      (void)font;
      ids.push_back(fontId);
   }
   std::sort(ids.begin(), ids.end());
   return ids;
}

TextLayoutResult FontManager::LayoutText(std::string_view text, const TextStyle& style) {
   return TextLayout::Build(*this, text, style);
}

const GlyphInfo* FontManager::GetOrCreateGlyph(const std::string& fontId, uint32_t pixelSize, char32_t codePoint) {
   const auto iterator = msdfFonts_.find(fontId);
   return iterator != msdfFonts_.end() ? iterator->second->GetGlyph(pixelSize, codePoint) : nullptr;
}

FontMetrics FontManager::GetMetrics(const std::string& fontId, uint32_t pixelSize) {
   const auto iterator = msdfFonts_.find(fontId);
   return iterator != msdfFonts_.end() ? iterator->second->GetMetrics(pixelSize) : FontMetrics{};
}

float FontManager::GetKerning(const std::string& fontId, uint32_t leftGlyph, uint32_t rightGlyph, uint32_t pixelSize) {
   const auto iterator = msdfFonts_.find(fontId);
   return iterator != msdfFonts_.end() ? iterator->second->GetKerning(leftGlyph, rightGlyph, pixelSize) : 0.0f;
}

void FontManager::ReleaseIntermediateResources() {
   // GPU転送完了後、各フォントのアトラス本体を残してアップロード領域だけを解放する。
   for (auto& [fontId, font] : msdfFonts_) {
      (void)fontId;
      font->ReleaseIntermediateResources();
   }
}

void FontManager::Clear() {
   msdfFonts_.clear();
   device_ = nullptr;
   // 全フォント消失も既存テキストの再レイアウト対象になるため、世代を更新する。
   ++revision_;
}

} // namespace GameEngine
