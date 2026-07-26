#pragma once

#include "Object/Component/IObjectComponent.h"

namespace App {

/// @brief Vehicle の速度を SpeedLine ポストエフェクトへ反映するコンポーネント
class VehicleSpeedPostEffectController final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "VehicleSpeedPostEffectController";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "車両速度ポストエフェクト", "Vehicle Speed Post Effect Controller" };
   /// @copydoc GameEngine::IObjectComponent::GetTypeName
   const char* GetTypeName() const override { return kTypeName; }
   /// @brief 所有中の共有ポストエフェクト状態を中立値へ戻して破棄する
   ~VehicleSpeedPostEffectController() override;

   /// @copydoc GameEngine::IObjectComponent::Update
   void Update(float deltaTime) override;
   /// @copydoc GameEngine::IObjectComponent::OnDetach
   void OnDetach() override;
   /// @copydoc GameEngine::IObjectComponent::OnDisable
   void OnDisable() override;

#ifdef USE_IMGUI
   /// @copydoc GameEngine::IObjectComponent::DrawInspector
   void DrawInspector() override;
#endif

   /// @copydoc GameEngine::IObjectComponent::Serialize
   nlohmann::json Serialize() const override;
   /// @copydoc GameEngine::IObjectComponent::Deserialize
   void Deserialize(const nlohmann::json& data) override;

public:
   /// @brief この速度に達すると SpeedLine 演出が出始める
   float minimumEffectSpeed = 16.0f;

   /// @brief この速度で SpeedLine 演出量が最大になる
   float maximumEffectSpeed = 40.0f;

   /// @brief 演出量の追従速度（per sec）
   float responseSpeed = 100.0f;

   /// @brief この演出量未満では SpeedLine を無効化する
   float visibleThreshold = 0.015f;

   /// @brief 最低速度付近の内側半径。ほぼ画面端なので見えない
   float idleInnerRadius = 0.98f;

   /// @brief 最高速演出時の内側半径。小さいほど中心側まで線が入る
   float activeInnerRadius = 0.4f;

   /// @brief SpeedLine の外側半径
   float outerRadius = 2.5f;

   /// @brief 最大演出時の明るさ
   float maxIntensity = 0.8f;

   /// @brief 最低速度付近の流速
   float idleFlowSpeed = 10.0f;

   /// @brief 最大演出時の流速
   float activeFlowSpeed = 20.0f;

   float lineDensity = 140.0f; ///< 画面内に生成する放射線の密度
   float thickness = 0.8f; ///< 放射線の太さ
   float randomSeed = 1.0f; ///< 放射線パターンを選ぶ乱数シード

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
