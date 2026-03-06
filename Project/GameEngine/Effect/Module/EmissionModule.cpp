#include "pch.h"
#include "EmissionModule.h"

namespace GameEngine {
    EmissionModule::EmissionModule() = default;

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
        if (j.contains("rateOverTime")) rateOverTime_ = j["rateOverTime"];
        if (j.contains("rateOverDistance")) rateOverDistance_ = j["rateOverDistance"];
        
        if (j.contains("bursts")) {
            bursts_.clear();
            for (const auto& burstJson : j["bursts"]) {
                Burst burst;
                burst.time = burstJson["time"];
                burst.count = burstJson["count"];
                burst.cycles = burstJson["cycles"];
                burst.interval = burstJson["interval"];
                bursts_.push_back(burst);
            }
        }
    }
}
