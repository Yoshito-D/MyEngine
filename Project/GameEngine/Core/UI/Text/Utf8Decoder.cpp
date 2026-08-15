#include "pch.h"
#include "Utf8Decoder.h"

namespace GameEngine {
namespace {
constexpr char32_t kReplacementCharacter = 0xFFFD;

bool IsContinuationByte(uint8_t value) {
   return (value & 0xC0u) == 0x80u;
}
}

std::vector<char32_t> DecodeUtf8(std::string_view text) {
   std::vector<char32_t> result;
   // ASCIIだけの場合の最大要素数を先に確保し、デコード中の再確保を避ける。
   result.reserve(text.size());

   size_t offset = 0;
   while (offset < text.size()) {
      const uint8_t first = static_cast<uint8_t>(text[offset]);
      char32_t codePoint = 0;
      size_t sequenceLength = 0;

      // 先頭バイトのビットパターンから系列長とコードポイント上位ビットを取り出す。
      if (first <= 0x7Fu) {
         codePoint = first;
         sequenceLength = 1;
      } else if ((first & 0xE0u) == 0xC0u) {
         codePoint = first & 0x1Fu;
         sequenceLength = 2;
      } else if ((first & 0xF0u) == 0xE0u) {
         codePoint = first & 0x0Fu;
         sequenceLength = 3;
      } else if ((first & 0xF8u) == 0xF0u) {
         codePoint = first & 0x07u;
         sequenceLength = 4;
      } else {
         result.push_back(kReplacementCharacter);
         ++offset;
         continue;
      }

      if (offset + sequenceLength > text.size()) {
         // 末尾で途切れた系列は残り全体を1つの置換文字として扱い、範囲外参照を避ける。
         result.push_back(kReplacementCharacter);
         break;
      }

      bool valid = true;
      // 継続バイトを6ビットずつ連結してUnicodeコードポイントを復元する。
      for (size_t index = 1; index < sequenceLength; ++index) {
         const uint8_t continuation = static_cast<uint8_t>(text[offset + index]);
         if (!IsContinuationByte(continuation)) {
            valid = false;
            break;
         }
         codePoint = (codePoint << 6u) | (continuation & 0x3Fu);
      }

      const bool isOverlong =
         (sequenceLength == 2 && codePoint < 0x80) ||
         (sequenceLength == 3 && codePoint < 0x800) ||
         (sequenceLength == 4 && codePoint < 0x10000);
      const bool isSurrogate = codePoint >= 0xD800 && codePoint <= 0xDFFF;
      const bool isOutOfRange = codePoint > 0x10FFFF;

      // 最短形式でない符号化、UTF-16専用領域、Unicode上限外は妥当なUTF-8として受理しない。
      if (!valid || isOverlong || isSurrogate || isOutOfRange) {
         result.push_back(kReplacementCharacter);
         // 壊れた先頭バイトだけを消費し、次の正しい文字境界へ早く再同期する。
         ++offset;
         continue;
      }

      result.push_back(codePoint);
      offset += sequenceLength;
   }

   return result;
}

size_t CountRenderableCodePoints(std::string_view text) {
   const auto codePoints = DecodeUtf8(text);
   // 改行コードは配置数から除外する一方、空白や置換文字は描画進行に必要なので数える。
   return static_cast<size_t>(std::count_if(codePoints.begin(), codePoints.end(), [](char32_t codePoint) {
      return codePoint != U'\n' && codePoint != U'\r';
   }));
}

} // namespace GameEngine
