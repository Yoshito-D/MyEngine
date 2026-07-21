#include "RaceTimeFormatting.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace App {

std::string FormatRaceTime(double seconds) {
   const auto totalMilliseconds = static_cast<long long>((std::max)(seconds, 0.0) * 1000.0);
   const long long minutes = totalMilliseconds / 60000;
   const long long remainder = totalMilliseconds % 60000;
   const long long wholeSeconds = remainder / 1000;
   const long long milliseconds = remainder % 1000;

   std::ostringstream stream;
   stream << std::setfill('0') << std::setw(2) << minutes << ':'
      << std::setw(2) << wholeSeconds << '.'
      << std::setw(3) << milliseconds;
   return stream.str();
}

} // namespace App
