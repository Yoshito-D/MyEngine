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
      float time = 0.0f;     // 発生時間
      uint32_t count = 0;    // 発生数
      uint32_t cycles = 1;   // 繰り返し回数（0 = 無限ループ）
      float interval = 1.0f; // 繰り返し間隔
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
   void ClearBursts();

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
   bool enabled_ = true;           ///< この機能を実行するかどうか。
   float rateOverTime_ = 10.0f;    ///< 1秒あたりに放出する粒子数。
   float rateOverDistance_ = 0.0f; ///< エミッターの移動距離1単位あたりに放出する粒子数。

   struct BurstCursor {
      uint32_t firedCount = 0;    ///< 現在の再生で発火済みのバースト回数。
      float nextFireTime = -1.0f; ///< 次のバーストを発火するシステム時刻（秒）。負値は未初期化。
   };

   static Burst ValidateBurst(Burst burst);
   std::vector<Burst> bursts_;            ///< 保存対象となるバーストの開始時刻・粒子数・周期設定。
   std::vector<BurstCursor> burstStates_; ///< バースト設定と同じ順序で保持する再生中の発火カーソル。
};
}
