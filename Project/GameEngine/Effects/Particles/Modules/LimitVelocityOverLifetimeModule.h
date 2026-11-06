#pragma once

#include "GameEngine/Effects/Particles/Modules/ParticleModule.h"
#include "GameEngine/Effects/Particles/Modules/MainModule.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
// ============================================================
// 生存期間中の速度制限モジュール
// ============================================================
class LimitVelocityOverLifetimeModule : public ParticleModule {
public:
   /// @brief 速度制限設定を既定値で初期化する
   LimitVelocityOverLifetimeModule();

   /// @brief ランダム値を粒子ごとに初期化
   void InitializeParticle(Particle& particle) const;

   /// @brief パーティクルの速度を制限
   /// @param deltaTime 更新時間（s）。0以下では速度を変更しない。
   void LimitVelocity(Particle& particle, float deltaTime) const;

   /// @brief シミュレーション空間を考慮して速度を制限する
   /// @param deltaTime 更新時間（s）。0以下では速度を変更しない。
   void LimitVelocity(
      Particle& particle, float deltaTime, const Transform& simulationTransform, bool useLocalSimulation) const;

   /// @brief 制限速度（m/s）を固定値で設定する
   void SetSpeedLimit(float limit) { speedLimit_ = RandomFloat(limit, limit, false); }

   /// @brief 制限速度（m/s）の代表値を取得する
   float GetSpeedLimit() const { return speedLimit_.Minimum(); }

   /// @brief 制限速度（m/s）を乱数範囲で設定する
   void SetSpeedLimitRange(const RandomFloat& limit) { speedLimit_ = limit; }

   /// @brief 制限速度（m/s）の乱数範囲を取得する
   const RandomFloat& GetSpeedLimitRange() const { return speedLimit_; }

   /// @brief 超過速度へ適用する減衰係数（1/s）を固定値で設定する
   void SetDampingRate(float dampingRate) { dampingRate_ = RandomFloat(dampingRate, dampingRate, false); }

   /// @brief 減衰係数（1/s）の代表値を取得する
   float GetDampingRate() const { return dampingRate_.Minimum(); }

   /// @brief 減衰係数（1/s）を乱数範囲で設定する
   void SetDampingRateRange(const RandomFloat& dampingRate) { dampingRate_ = dampingRate; }

   /// @brief 減衰係数（1/s）の乱数範囲を取得する
   const RandomFloat& GetDampingRateRange() const { return dampingRate_; }

   /// @brief 制限速度（m/s）と減衰係数（1/s）をJSONへ保存する。
   nlohmann::json ToJson() const override;

   /// @brief 制限速度と減衰係数の乱数範囲をJSONから読み込む。
   void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI

   /// @brief 制限速度と減衰係数を単位付きで編集する。
   void DrawInspector() override;
#endif

private:
   RandomFloat speedLimit_{10.0f, 10.0f, false};          ///< 制限速度（m/s）の乱数範囲。
   RandomFloat dampingRate_{41.58883f, 41.58883f, false}; ///< 減衰係数（1/s）の乱数範囲。
};
}
