#pragma once

#include "GameEngine/Effects/Particles/Modules/ParticleModule.h"
#include "GameEngine/Effects/Particles/Modules/MainModule.h"
#include "GameEngine/Math/VectorMath.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
// ============================================================
// 生存期間中の速度モジュール
// ============================================================
class VelocityOverLifetimeModule : public ParticleModule {
public:
   /// @brief 生存期間速度設定を既定値で初期化する
   VelocityOverLifetimeModule();

   /// @brief ランダム値を粒子ごとに初期化
   void InitializeParticle(Particle& particle) const;

   /// @brief パーティクルの速度を更新
   void ApplyVelocity(Particle& particle, float deltaTime) const;

   /// @brief シミュレーション空間を考慮して速度を更新する
   void ApplyVelocity(
      Particle& particle, float deltaTime, const Transform& simulationTransform, bool useLocalSimulation) const;

   /// @brief 加速度（m/s²）を固定値で設定する
   void SetLinearAcceleration(const Vector3& acceleration) {
      linearAcceleration_ = RandomVector3(acceleration, acceleration, false);
   }

   /// @brief 加速度（m/s²）の代表値を取得する
   Vector3 GetLinearAcceleration() const { return linearAcceleration_.Minimum(); }

   /// @brief 加速度（m/s²）を乱数範囲で設定する
   void SetLinearAccelerationRange(const RandomVector3& acceleration) { linearAcceleration_ = acceleration; }

   /// @brief 加速度（m/s²）の乱数範囲を取得する
   const RandomVector3& GetLinearAccelerationRange() const { return linearAcceleration_; }

   /// @brief 速度の指数増幅率（1/s、0で等速）を固定値で設定する
   void SetSpeedGrowthRate(float rate) { speedGrowthRate_ = RandomFloat(rate, rate, false); }

   /// @brief 速度の指数増幅率（1/s、0で等速）の代表値を取得する
   float GetSpeedGrowthRate() const { return speedGrowthRate_.Minimum(); }

   /// @brief 速度の指数増幅率（1/s、0で等速）を乱数範囲で設定する
   void SetSpeedGrowthRateRange(const RandomFloat& rate) { speedGrowthRate_ = rate; }

   /// @brief 速度の指数増幅率（1/s、0で等速）の乱数範囲を取得する
   const RandomFloat& GetSpeedGrowthRateRange() const { return speedGrowthRate_; }

   /// @brief 加速度（m/s²）と指数増幅率（1/s）を乱数範囲としてJSONへ保存する。
   nlohmann::json ToJson() const override;

   /// @brief 加速度と指数増幅率の設定JSONを読み込む。
   void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI

   /// @brief 加速度と指数増幅率を単位付きで編集する。
   void DrawInspector() override;
#endif

private:
   /// @brief 加速度（m/s²）の乱数範囲。
   RandomVector3 linearAcceleration_{Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), false};
   RandomFloat speedGrowthRate_{0.0f, 0.0f, false}; ///< 速度の指数増幅率の乱数範囲（1/s）。
};
}
