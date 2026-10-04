#pragma once
#include <cmath>
#include <algorithm>

#include "GameEngine/Object/Component/Base/IObjectComponent.h"

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

   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings {
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
   };
   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const {
      Settings settings;
      settings.minimumEffectSpeed = minimumEffectSpeed;
      settings.maximumEffectSpeed = maximumEffectSpeed;
      settings.responseSpeed = responseSpeed;
      settings.visibleThreshold = visibleThreshold;
      settings.idleInnerRadius = idleInnerRadius;
      settings.activeInnerRadius = activeInnerRadius;
      settings.outerRadius = outerRadius;
      settings.maxIntensity = maxIntensity;
      settings.idleFlowSpeed = idleFlowSpeed;
      settings.activeFlowSpeed = activeFlowSpeed;
      settings.lineDensity = lineDensity;
      settings.thickness = thickness;
      settings.randomSeed = randomSeed;
      settings.radialBlurMaxStrength = radialBlurMaxStrength;
      settings.radialBlurSampleCount = radialBlurSampleCount;
      settings.radialBlurVisibleThreshold = radialBlurVisibleThreshold;
      return settings;
   }
   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested) {
      auto settings = requested;
      [[maybe_unused]] const Settings defaults;
      if (!std::isfinite(settings.minimumEffectSpeed)) settings.minimumEffectSpeed = defaults.minimumEffectSpeed;
      if (!std::isfinite(settings.maximumEffectSpeed)) settings.maximumEffectSpeed = defaults.maximumEffectSpeed;
      if (!std::isfinite(settings.responseSpeed)) settings.responseSpeed = defaults.responseSpeed;
      if (!std::isfinite(settings.visibleThreshold)) settings.visibleThreshold = defaults.visibleThreshold;
      if (!std::isfinite(settings.idleInnerRadius)) settings.idleInnerRadius = defaults.idleInnerRadius;
      if (!std::isfinite(settings.activeInnerRadius)) settings.activeInnerRadius = defaults.activeInnerRadius;
      if (!std::isfinite(settings.outerRadius)) settings.outerRadius = defaults.outerRadius;
      if (!std::isfinite(settings.maxIntensity)) settings.maxIntensity = defaults.maxIntensity;
      if (!std::isfinite(settings.idleFlowSpeed)) settings.idleFlowSpeed = defaults.idleFlowSpeed;
      if (!std::isfinite(settings.activeFlowSpeed)) settings.activeFlowSpeed = defaults.activeFlowSpeed;
      if (!std::isfinite(settings.lineDensity)) settings.lineDensity = defaults.lineDensity;
      if (!std::isfinite(settings.thickness)) settings.thickness = defaults.thickness;
      if (!std::isfinite(settings.randomSeed)) settings.randomSeed = defaults.randomSeed;
      if (!std::isfinite(settings.radialBlurMaxStrength)) settings.radialBlurMaxStrength = defaults.radialBlurMaxStrength;
      if (!std::isfinite(settings.radialBlurVisibleThreshold)) settings.radialBlurVisibleThreshold = defaults.radialBlurVisibleThreshold;
      settings.maximumEffectSpeed = std::max(settings.minimumEffectSpeed + 0.001f, settings.maximumEffectSpeed);
      settings.responseSpeed = std::max(0.0f, settings.responseSpeed);
      settings.radialBlurSampleCount = std::clamp(settings.radialBlurSampleCount, 1, 64);
      minimumEffectSpeed = settings.minimumEffectSpeed;
      maximumEffectSpeed = settings.maximumEffectSpeed;
      responseSpeed = settings.responseSpeed;
      visibleThreshold = settings.visibleThreshold;
      idleInnerRadius = settings.idleInnerRadius;
      activeInnerRadius = settings.activeInnerRadius;
      outerRadius = settings.outerRadius;
      maxIntensity = settings.maxIntensity;
      idleFlowSpeed = settings.idleFlowSpeed;
      activeFlowSpeed = settings.activeFlowSpeed;
      lineDensity = settings.lineDensity;
      thickness = settings.thickness;
      randomSeed = settings.randomSeed;
      radialBlurMaxStrength = settings.radialBlurMaxStrength;
      radialBlurSampleCount = settings.radialBlurSampleCount;
      radialBlurVisibleThreshold = settings.radialBlurVisibleThreshold;
   }

private:
   float minimumEffectSpeed = 16.0f;
   float maximumEffectSpeed = 40.0f;
   float responseSpeed = 100.0f;
   float visibleThreshold = 0.015f;
   float idleInnerRadius = 0.98f;
   float activeInnerRadius = 0.4f;
   float outerRadius = 2.5f;
   float maxIntensity = 0.8f;
   float idleFlowSpeed = 10.0f;
   float activeFlowSpeed = 20.0f;
   float lineDensity = 140.0f; ///< 画面内に生成する放射線の密度
   float thickness = 0.8f; ///< 放射線の太さ
   float randomSeed = 1.0f; ///< 放射線パターンを選ぶ乱数シード
   float radialBlurMaxStrength = 0.05f;
   int radialBlurSampleCount = 16;
   float radialBlurVisibleThreshold = 0.02f;

private:
   void ApplyNeutralEffect();

   float effectAmount_ = 0.0f;
   float time_ = 0.0f;
};

} // namespace App
