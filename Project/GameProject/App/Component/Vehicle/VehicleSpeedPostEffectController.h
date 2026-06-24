#pragma once

#include "Object/Component/IObjectComponent.h"

namespace App {

/// @brief Vehicle の速度差を SpeedLine ポストエフェクトへ反映するコンポーネント
class VehicleSpeedPostEffectController final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "VehicleSpeedPostEffectController";
   const char* GetTypeName() const override { return kTypeName; }
   ~VehicleSpeedPostEffectController() override;

   void Update(float deltaTime) override;
   void OnDetach() override;
   void OnDisable() override;

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   nlohmann::json Serialize() const override;
   void Deserialize(const nlohmann::json& data) override;

public:
   /// @brief autoSpeed からこの分だけ超えるまでは非表示扱いにする
   float activationMargin = -20.0f;

   /// @brief この速度差で SpeedLine 演出量が最大になる
   float effectSpeedRange = 20.0f;

   /// @brief 演出量の追従速度（per sec）
   float responseSpeed = 100.0f;

   /// @brief この演出量未満では SpeedLine を無効化する
   float visibleThreshold = 0.015f;

   /// @brief autoSpeed 付近の内側半径。ほぼ画面端なので見えない
   float idleInnerRadius = 0.98f;

   /// @brief 最高速演出時の内側半径。小さいほど中心側まで線が入る
   float activeInnerRadius = 0.4f;

   /// @brief SpeedLine の外側半径
   float outerRadius = 2.5f;

   /// @brief 最大演出時の明るさ
   float maxIntensity = 0.8f;

   /// @brief autoSpeed 付近の流速
   float idleFlowSpeed = 10.0f;

   /// @brief 最大演出時の流速
   float activeFlowSpeed = 20.0f;

   float lineDensity = 140.0f;
   float thickness = 0.8f;
   float randomSeed = 1.0f;

   /// @brief 最大演出時の放射ブラー強度
   float radialBlurMaxStrength = 0.05f;

   /// @brief 放射ブラーのサンプル数
   int radialBlurSampleCount = 16;

   /// @brief この演出量未満では RadialBlur を無効化する
   float radialBlurVisibleThreshold = 0.02f;

private:
   void ApplyNeutralEffect();

   float effectAmount_ = 0.0f;
   float time_ = 0.0f;
};

} // namespace App
