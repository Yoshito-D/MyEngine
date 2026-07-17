#include "pch.h"
#include "FontFace.h"
#include <fstream>

namespace GameEngine {

FontFace::~FontFace() {
   if (face_) {
      FT_Done_Face(face_);
      face_ = nullptr;
   }
}

bool FontFace::Load(FT_Library library, const std::filesystem::path& filePath) {
   if (!library) {
      Logger::Error("[FontFace] FreeType library is not initialized.");
      return false;
   }

   std::ifstream file(filePath, std::ios::binary | std::ios::ate);
   if (!file.is_open()) {
      Logger::Error("[FontFace] Failed to open font: " + filePath.generic_string());
      return false;
   }

   const std::streamsize fileSize = file.tellg();
   if (fileSize <= 0) {
      Logger::Error("[FontFace] Font file is empty: " + filePath.generic_string());
      return false;
   }

   fontBytes_.resize(static_cast<size_t>(fileSize));
   file.seekg(0, std::ios::beg);
   if (!file.read(reinterpret_cast<char*>(fontBytes_.data()), fileSize)) {
      Logger::Error("[FontFace] Failed to read font: " + filePath.generic_string());
      fontBytes_.clear();
      return false;
   }

   if (face_) {
      FT_Done_Face(face_);
      face_ = nullptr;
   }

   const FT_Error error = FT_New_Memory_Face(
      library,
      fontBytes_.data(),
      static_cast<FT_Long>(fontBytes_.size()),
      0,
      &face_);
   if (error != FT_Err_Ok) {
      Logger::Error("[FontFace] FT_New_Memory_Face failed: error=" + std::to_string(error));
      fontBytes_.clear();
      return false;
   }

   currentPixelSize_ = 0;
   return true;
}

bool FontFace::RasterizeGlyph(char32_t codePoint, uint32_t pixelSize, RasterizedGlyph& output) {
   output = {};
   if (!face_ || !SetPixelSize(pixelSize)) {
      return false;
   }

   const FT_UInt glyphIndex = FT_Get_Char_Index(face_, static_cast<FT_ULong>(codePoint));
   if (glyphIndex == 0 && codePoint != 0) {
      return false;
   }

   const FT_Error error = FT_Load_Glyph(face_, glyphIndex, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL);
   if (error != FT_Err_Ok) {
      Logger::Warning("[FontFace] FT_Load_Glyph failed: error=" + std::to_string(error));
      return false;
   }

   const FT_GlyphSlot slot = face_->glyph;
   const FT_Bitmap& bitmap = slot->bitmap;
   output.width = bitmap.width;
   output.height = bitmap.rows;
   output.bitmapLeft = slot->bitmap_left;
   output.bitmapTop = slot->bitmap_top;
   output.advanceX = static_cast<float>(slot->advance.x) / 64.0f;
   output.glyphIndex = glyphIndex;

   if (output.width == 0 || output.height == 0) {
      return true;
   }

   output.pixels.resize(static_cast<size_t>(output.width) * output.height);
   const int pitch = bitmap.pitch;
   const size_t absolutePitch = static_cast<size_t>(std::abs(pitch));

   for (uint32_t row = 0; row < output.height; ++row) {
      const uint32_t sourceRow = pitch >= 0 ? row : (output.height - 1u - row);
      const uint8_t* source = bitmap.buffer + static_cast<size_t>(sourceRow) * absolutePitch;
      uint8_t* destination = output.pixels.data() + static_cast<size_t>(row) * output.width;

      if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
         std::memcpy(destination, source, output.width);
      } else if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
         for (uint32_t column = 0; column < output.width; ++column) {
            const uint8_t bit = static_cast<uint8_t>(0x80u >> (column & 7u));
            destination[column] = (source[column >> 3u] & bit) != 0 ? 255u : 0u;
         }
      } else {
         Logger::Warning("[FontFace] Unsupported glyph pixel mode: " + std::to_string(bitmap.pixel_mode));
         return false;
      }
   }

   return true;
}

FontMetrics FontFace::GetMetrics(uint32_t pixelSize) {
   FontMetrics metrics{};
   if (!face_ || !SetPixelSize(pixelSize) || !face_->size) {
      return metrics;
   }

   metrics.ascender = static_cast<float>(face_->size->metrics.ascender) / 64.0f;
   metrics.descender = static_cast<float>(face_->size->metrics.descender) / 64.0f;
   metrics.lineHeight = static_cast<float>(face_->size->metrics.height) / 64.0f;
   if (metrics.lineHeight <= 0.0f) {
      metrics.lineHeight = metrics.ascender - metrics.descender;
   }
   return metrics;
}

float FontFace::GetKerning(uint32_t leftGlyph, uint32_t rightGlyph, uint32_t pixelSize) {
   if (!face_ || leftGlyph == 0 || rightGlyph == 0 || !FT_HAS_KERNING(face_) || !SetPixelSize(pixelSize)) {
      return 0.0f;
   }

   FT_Vector kerning{};
   if (FT_Get_Kerning(face_, leftGlyph, rightGlyph, FT_KERNING_DEFAULT, &kerning) != FT_Err_Ok) {
      return 0.0f;
   }
   return static_cast<float>(kerning.x) / 64.0f;
}

bool FontFace::HasGlyph(char32_t codePoint) const {
   return face_ && FT_Get_Char_Index(face_, static_cast<FT_ULong>(codePoint)) != 0;
}

bool FontFace::SetPixelSize(uint32_t pixelSize) {
   if (!face_ || pixelSize == 0) {
      return false;
   }
   if (currentPixelSize_ == pixelSize) {
      return true;
   }

   const FT_Error error = FT_Set_Pixel_Sizes(face_, 0, pixelSize);
   if (error != FT_Err_Ok) {
      Logger::Error("[FontFace] FT_Set_Pixel_Sizes failed: error=" + std::to_string(error));
      return false;
   }
   currentPixelSize_ = pixelSize;
   return true;
}

} // namespace GameEngine
