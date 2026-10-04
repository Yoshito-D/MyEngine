#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <nlohmann/json.hpp>
#include "GameEngine/Effects/Particles/Modules/ParticleModule.h"

namespace GameEngine {
    // ============================================================
    // Emission Module (放出モジュール)
    // パーティクルの生成方法を制御
    // ============================================================
    class EmissionModule {
    public:
        struct Burst {
            float time = 0.0f;          // 発生時間
            uint32_t count = 0;      // 発生数
            uint32_t cycles = 1;     // 繰り返し回数（0 = 無限ループ）
            float interval = 1.0f;      // 繰り返し間隔

        };

        EmissionModule();

        void SetEnabled(bool enabled) { enabled_ = enabled; }
        bool IsEnabled() const { return enabled_; }

        // 時間による放出率
        void SetRateOverTime(float rate) { rateOverTime_ = std::isfinite(rate) ? std::max(0.0f, rate) : 0.0f; }
        float GetRateOverTime() const { return rateOverTime_; }

        // 距離による放出率
        void SetRateOverDistance(float rate) { rateOverDistance_ = std::isfinite(rate) ? std::max(0.0f, rate) : 0.0f; }
        float GetRateOverDistance() const { return rateOverDistance_; }

        // バースト
        /// @brief 設定を検証し、再生カーソルと同時に追加する。
        void AddBurst(const Burst& burst);
        /// @brief 設定と再生カーソルをまとめて破棄する。
        void ClearBursts() { bursts_.clear(); burstStates_.clear(); }
        /// @brief 保存用の放出設定だけを読み取る。
        const std::vector<Burst>& GetBursts() const { return bursts_; }
        /// @brief 設定全体を検証して置換し、再生カーソルを再構築する
        void ReplaceBurstSchedule(const std::vector<Burst>& bursts);
        /// @brief 指定時刻まで放出スケジュールを進め、今回の放出数を返す
        uint32_t AdvanceBursts(float time);

        /// @brief Burst の発火状態をリセット（Play/Stop 時に呼ぶ）
        void ResetBurstStates();

        // JSONシリアライズ
        nlohmann::json ToJson() const;
        void FromJson(const nlohmann::json& json);

#ifdef USE_IMGUI
        void DrawInspector();
#endif

    private:
        bool enabled_ = true;
        float rateOverTime_ = 10.0f;
        float rateOverDistance_ = 0.0f;
        struct BurstCursor { uint32_t firedCount = 0; float nextFireTime = -1.0f; };
        static Burst ValidateBurst(Burst burst);
        std::vector<Burst> bursts_;
        std::vector<BurstCursor> burstStates_;
    };
}
