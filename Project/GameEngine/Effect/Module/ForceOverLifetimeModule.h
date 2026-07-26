#pragma once
#include "ParticleModule.h"
#include "MainModule.h"
#include "Utility/VectorMath.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
// ============================================================
// Force over Lifetime Module
// ============================================================
class ForceOverLifetimeModule : public ParticleModule {
public:
   /// @brief 生存期間フォース設定を既定値で初期化する
   ForceOverLifetimeModule();

   /// @brief ランダム値を粒子ごとに初期化
   void InitializeParticle(Particle& particle) const;

   /// @brief パーティクルに力を適用
   void ApplyForce(Particle& particle) const;
   /// @brief シミュレーション空間を考慮して力を適用する
   void ApplyForce(Particle& particle, const Transform& simulationTransform, bool useLocalSimulation) const;

   /// @brief デルタタイムを考慮してパーティクルに力と空気抵抗を適用する
   /// @param particle 対象パーティクル
   /// @param deltaTime シミュレーションのデルタタイム
   /// @param simulationTransform ローカル空間の基準トランスフォーム
   /// @param useLocalSimulation ローカル空間として解釈するか
   void ApplyForce(Particle& particle, float deltaTime, const Transform& simulationTransform, bool useLocalSimulation) const;

   /// @brief 加える力を固定値で設定する
   void SetForce(const Vector3& force) { force_ = RandomVector3(force, force, false); }
   /// @brief 加える力の代表値を取得する
   const Vector3& GetForce() const { return force_.minValue; }
   /// @brief 加える力を乱数範囲で設定する
   void SetForceRange(const RandomVector3& force) { force_ = force; }
   /// @brief 加える力の乱数範囲を取得する
   const RandomVector3& GetForceRange() const { return force_; }

   /// @brief 速度に比例する空気抵抗係数を設定する
   void SetDrag(float drag) { drag_ = RandomFloat(std::max(drag, 0.0f)); }

   /// @brief 空気抵抗係数を取得する
   float GetDrag() const { return drag_.minValue; }

   /// @brief 粒子ごとの空気抵抗範囲を設定する
   void SetDragRange(const RandomFloat& drag) { drag_ = drag; }

   /// @brief 粒子ごとの空気抵抗範囲を取得する
   const RandomFloat& GetDragRange() const { return drag_; }

   /// @brief ポイント引力または斥力を有効化する
   void SetAttractorEnabled(bool enabled) { attractorEnabled_ = enabled; }

   /// @brief ポイント引力または斥力が有効か取得する
   bool IsAttractorEnabled() const { return attractorEnabled_; }

   /// @brief 引力中心を設定する
   void SetAttractorPosition(const Vector3& position) { attractorPosition_ = position; }

   /// @brief 引力中心を取得する
   const Vector3& GetAttractorPosition() const { return attractorPosition_; }

   /// @brief 引力強度を設定する
   /// @param strength 正で引力、負で斥力
   void SetAttractorStrength(float strength) { attractorStrength_ = strength; }

   /// @brief 引力強度を取得する
   float GetAttractorStrength() const { return attractorStrength_; }

   /// @brief 作用半径を設定する
   /// @param radius 0なら距離制限なし
   void SetAttractorRadius(float radius) { attractorRadius_ = std::max(radius, 0.0f); }

   /// @brief 作用半径を取得する
   float GetAttractorRadius() const { return attractorRadius_; }

   /// @brief 距離減衰指数を設定する
   void SetAttractorFalloff(float falloff) { attractorFalloff_ = std::max(falloff, 0.0f); }

   /// @brief 距離減衰指数を取得する
   float GetAttractorFalloff() const { return attractorFalloff_; }

   /// @copydoc ParticleModule::ToJson
   nlohmann::json ToJson() const override;
   /// @copydoc ParticleModule::FromJson
   void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI
   /// @copydoc ParticleModule::DrawInspector
   void DrawInspector() override;
#endif

private:
   RandomVector3 force_{ Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), false };
   RandomFloat drag_{ 0.0f, 0.0f, false };
   bool attractorEnabled_ = false;
   Vector3 attractorPosition_{ 0.0f, 0.0f, 0.0f };
   float attractorStrength_ = 0.0f;
   float attractorRadius_ = 0.0f;
   float attractorFalloff_ = 1.0f;
};
}
