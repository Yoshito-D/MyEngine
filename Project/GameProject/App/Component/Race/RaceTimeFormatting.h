#pragma once

#include <string>

namespace App {

/// @brief 秒単位のレースタイムをMM:SS.mmm形式へ変換する
/// @param seconds 秒単位の時間。負値は0として扱う
/// @return UI表示用の時間文字列
std::string FormatRaceTime(double seconds);

} // namespace App
