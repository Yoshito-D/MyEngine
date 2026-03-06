#pragma once
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "ParticleModule.h"

namespace GameEngine {
    // ============================================================
    // Emission Module (放出モジュール)
    // パーティクルの生成方法を制御
    // ============================================================
    class EmissionModule {
    public:
        struct Burst {
            float time;          // 発生時間
            uint32_t count;      // 発生数
            uint32_t cycles;     // 繰り返し回数
            float interval;      // 繰り返し間隔
        };

        EmissionModule();

        void SetEnabled(bool enabled) { enabled_ = enabled; }
        bool IsEnabled() const { return enabled_; }

        // Rate over Time
        void SetRateOverTime(float rate) { rateOverTime_ = rate; }
        float GetRateOverTime() const { return rateOverTime_; }

        // Rate over Distance
        void SetRateOverDistance(float rate) { rateOverDistance_ = rate; }
        float GetRateOverDistance() const { return rateOverDistance_; }

        // Bursts
        void AddBurst(const Burst& burst) { bursts_.push_back(burst); }
        void ClearBursts() { bursts_.clear(); }
        const std::vector<Burst>& GetBursts() const { return bursts_; }

        // JSON Serialization
        nlohmann::json ToJson() const;
        void FromJson(const nlohmann::json& json);

    private:
        bool enabled_ = true;
        float rateOverTime_ = 10.0f;
        float rateOverDistance_ = 0.0f;
        std::vector<Burst> bursts_;
    };
}
