#include "RaceTimeFormatting.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace App {

/// @brief 秒単位のレースタイムを固定幅の分・秒・ミリ秒表記へ変換する
/// @param seconds 秒単位の時間。負値は0として扱い、ミリ秒未満は切り捨てる
/// @return MM:SS.mmm形式のUI表示文字列
std::string FormatRaceTime(double seconds) {
   // 浮動小数のまま各桁を求めると境界で桁の不整合が起こり得るため、
   // 非負の総ミリ秒へ一度正規化してから整数演算で分解する。
   const auto totalMilliseconds = static_cast<long long>(std::max(seconds, 0.0) * 1000.0);
   const long long minutes = totalMilliseconds / 60000;
   const long long remainder = totalMilliseconds % 60000;
   const long long wholeSeconds = remainder / 1000;
   const long long milliseconds = remainder % 1000;

   // setwは最小幅の指定なので、100分以上のタイムも上位桁を失わず表示できる。
   std::ostringstream stream;
   stream << std::setfill('0') << std::setw(2) << minutes << ':'
      << std::setw(2) << wholeSeconds << '.'
      << std::setw(3) << milliseconds;
   return stream.str();
}

} // namespace App
