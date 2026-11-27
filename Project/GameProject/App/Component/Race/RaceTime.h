#pragma once

#include <cstdint>
#include <limits>

namespace App {

constexpr std::int64_t kRaceMillisecondsPerSecond = 1000; ///< レースタイムの秒からミリ秒への換算係数。

/// @brief 非負の走行時間を符号付き64ビットの総ミリ秒に変換できるかを確認する。
/// @param seconds JSONの読込時に狭める前の値も受け取るため、倍精度の秒数を使う。
/// @return ミリ秒への変換が範囲内に収まる場合はtrue。
constexpr bool IsRaceTimeRepresentable(double seconds) {
   // 整数上限をdoubleにすると2の63乗へ丸められるため、その値自体は含めない。
   constexpr double kExclusiveMillisecondsLimit = static_cast<double>(std::numeric_limits<std::int64_t>::max());
   return seconds >= 0.0 && seconds * kRaceMillisecondsPerSecond < kExclusiveMillisecondsLimit;
}

} // namespace App
