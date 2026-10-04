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
        for (auto burst : bursts) bursts_.push_back(ValidateBurst(burst));
        ResetBurstStates();
    }
    void EmissionModule::ResetBurstStates() {
        // 保存される設定と再生セッション固有のカーソルを分け、更新手順を外へ渡さない。
        burstStates_.assign(bursts_.size(), BurstCursor{});
    }
    uint32_t EmissionModule::AdvanceBursts(float time) {
        if (!enabled_ || !std::isfinite(time)) return 0;
        uint64_t count = 0;
        for (size_t i = 0; i < bursts_.size(); ++i) {
            const auto& burst = bursts_[i];
            auto& cursor = burstStates_[i];
            if (cursor.nextFireTime < 0.0f) cursor.nextFireTime = burst.time;
            const bool isInfinite = burst.cycles == 0;
            // 既存のfloat加算と発火順を維持し、カーソル更新だけを所有側へ移す。
            while (time >= cursor.nextFireTime && (isInfinite || cursor.firedCount < burst.cycles)) {
                count = std::min<uint64_t>(UINT32_MAX, count + burst.count);
                ++cursor.firedCount;
                cursor.nextFireTime += burst.interval > 0.0f ? burst.interval : std::numeric_limits<float>::max();
            }
        }
        return static_cast<uint32_t>(count);
    }

    nlohmann::json EmissionModule::ToJson() const {
        nlohmann::json j;
        
        j["enabled"] = enabled_;
        j["rateOverTime"] = rateOverTime_;
        j["rateOverDistance"] = rateOverDistance_;
        
        auto& burstsArray = j["bursts"] = nlohmann::json::array();
        for (const auto& burst : bursts_) {
            burstsArray.push_back({
                {"time", burst.time},
                {"count", burst.count},
                {"cycles", burst.cycles},
                {"interval", burst.interval}
            });
        }
        
        return j;
    }

    void EmissionModule::FromJson(const nlohmann::json& j) {
        if (j.contains("enabled")) enabled_ = j["enabled"];
        if (j.contains("rateOverTime")) SetRateOverTime(j["rateOverTime"]);
        if (j.contains("rateOverDistance")) SetRateOverDistance(j["rateOverDistance"]);
        
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
