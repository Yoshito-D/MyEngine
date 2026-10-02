#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace GameEngine {

/// @brief UTF-8文字列をUnicodeコードポイント列へ変換する
/// @param text UTF-8文字列
/// @return 不正なシーケンスをU+FFFDへ置換したコードポイント列
std::vector<char32_t> DecodeUtf8(std::string_view text);

/// @brief UI上で文字送りの対象になるコードポイント数を数える
/// @param text UTF-8文字列
/// @return 改行コードを除いたコードポイント数
size_t CountRenderableCodePoints(std::string_view text);

} // namespace GameEngine
