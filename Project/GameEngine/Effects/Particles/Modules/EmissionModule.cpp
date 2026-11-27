#include "GameEngine/pch.h"
#include <cmath>
#include <limits>
#include "GameEngine/Effects/Particles/Modules/EmissionModule.h"

namespace GameEngine {
EmissionModule::EmissionModule() = default;

EmissionModule::Burst EmissionModule::ValidateBurst(Burst burst) {
   burst.time = std::isfinite(burst.time) ? std::max(0.0f, burst.time) : 0.0f;
   burst.interval = std::isfinite(burst.interval) ? std::max(0.0f, burst.interval) : 0.0f;
   return burst;
}

void EmissionModule::AddBurst(const Burst& burst) {
   bursts_.push_back(ValidateBurst(burst));
   burstStates_.emplace_back();
}

void EmissionModule::ReplaceBurstSchedule(const std::vector<Burst>& bursts) {
   bursts_.clear();

   for (auto burst : bursts) {
      bursts_.push_back(ValidateBurst(burst));
   }

   ResetBurstStates();
}

void EmissionModule::ResetBurstStates() {
   // 保存される設定と再生セッション固有のカーソルを分け、更新手順を外へ渡さない。
   burstStates_.assign(bursts_.size(), BurstCursor{});
}

uint32_t EmissionModule::AdvanceBursts(float time) {
   if (!enabled_ || !std::isfinite(time)) {
      return 0;
   }

   constexpr auto kMaxRequestedParticles = std::numeric_limits<uint32_t>::max();
   uint32_t count = 0;

   for (size_t i = 0; i < bursts_.size(); ++i) {
      const auto& burst = bursts_[i];
      auto& cursor = burstStates_[i];

      if (time < burst.time) {
         continue;
      }

      // 開始時刻から発火回数を直接求め、時刻が大きく飛んでも過去の周期を一つずつ走査しない。
      const double elapsed = static_cast<double>(time) - burst.time;
      double due = burst.interval > 0.0f ? std::floor(elapsed / burst.interval) + 1.0 : 1.0;

      if (burst.cycles != 0) {
         due = std::min(due, static_cast<double>(burst.cycles));
      }

      if (due <= cursor.firedCount) {
         continue;
      }

      const double requested = (due - cursor.firedCount) * burst.count;
      cursor.firedCount = due;
      count += static_cast<uint32_t>(std::min(requested, static_cast<double>(kMaxRequestedParticles - count)));
   }

   return count;
}

nlohmann::json EmissionModule::ToJson() const {
   nlohmann::json j;

   j["enabled"] = enabled_;
   j["rateOverTime"] = rateOverTime_;
   j["rateOverDistance"] = rateOverDistance_;

   auto& burstsArray = j["bursts"] = nlohmann::json::array();

   for (const auto& burst : bursts_) {
      burstsArray.push_back(
         {{"time", burst.time}, {"count", burst.count}, {"cycles", burst.cycles}, {"interval", burst.interval}});
   }

   return j;
}

void EmissionModule::FromJson(const nlohmann::json& j) {
   if (j.contains("enabled")) {
      enabled_ = j["enabled"];
   }

   if (j.contains("rateOverTime")) {
      SetRateOverTime(j["rateOverTime"]);
   }

   if (j.contains("rateOverDistance")) {
      SetRateOverDistance(j["rateOverDistance"]);
   }

   if (j.contains("bursts")) {
      ClearBursts();

      for (const auto& burstJson : j["bursts"]) {
         Burst burst;
         burst.time = burstJson["time"];
         burst.count = burstJson["count"];
         burst.cycles = burstJson["cycles"];
         burst.interval = burstJson["interval"];
         AddBurst(burst);
      }
   }
}
}

namespace GameEngine {

void EmissionModule::ClearBursts() {
   bursts_.clear();
   burstStates_.clear();
}

} // namespace GameEngine
