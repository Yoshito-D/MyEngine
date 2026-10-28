#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

#include "GameEngine/Object/Component/Base/IObjectComponent.h"

namespace App {

/// @brief Vehicle の速度を SpeedLine ポストエフェクトへ反映するコンポーネント
class VehicleSpeedPostEffectController final : public GameEngine::IObjectComponent {
public:
   /// @brief シーン保存とファクトリ検索で使うコンポーネント型名。
   static constexpr const char* kTypeName = "VehicleSpeedPostEffectController";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{
      "車両速度ポストエフェクト", "Vehicle Speed Post Effect Controller"};

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

   /// @copydoc GameEngine::ParameterComponent::GetSharedParameters
   nlohmann::json GetSharedParameters() const override;

   /// @copydoc GameEngine::ParameterComponent::ApplySharedParameters
   void ApplySharedParameters(const nlohmann::json& data) override;

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
      float thickness = 0.8f;     ///< 放射線の太さ
      float randomSeed = 1.0f;    ///< 放射線パターンを選ぶ乱数シード

      /// @brief 最大演出時の放射ブラー強度
      float radialBlurMaxStrength = 0.05f;

      /// @brief 放射ブラーのサンプル数
      std::int32_t radialBlurSampleCount = 16;

      /// @brief この演出量未満では RadialBlur を無効化する
      float radialBlurVisibleThreshold = 0.02f;
   };

   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const;

   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested);

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   float minimumEffectSpeed = 16.0f;         ///< この速度に達すると SpeedLine 演出が出始める。
   float maximumEffectSpeed = 40.0f;         ///< この速度で SpeedLine 演出量が最大になる。
   float responseSpeed = 100.0f;             ///< 演出量の追従速度（per sec）。
   float visibleThreshold = 0.015f;          ///< この演出量未満では SpeedLine を無効化する。
   float idleInnerRadius = 0.98f;            ///< 最低速度付近の内側半径。ほぼ画面端なので見えない。
   float activeInnerRadius = 0.4f;           ///< 最高速演出時の内側半径。小さいほど中心側まで線が入る。
   float outerRadius = 2.5f;                 ///< 線を描き終える外側半径。
   float maxIntensity = 0.8f;                ///< 最大演出時の明るさ。
   float idleFlowSpeed = 10.0f;              ///< 最低速度付近の流速。
   float activeFlowSpeed = 20.0f;            ///< 最大演出時の流速。
   float lineDensity = 140.0f;               ///< 画面内に生成する放射線の密度
   float thickness = 0.8f;                   ///< 放射線の太さ
   float randomSeed = 1.0f;                  ///< 放射線パターンを選ぶ乱数シード
   float radialBlurMaxStrength = 0.05f;      ///< 最大演出時の放射ブラー強度。
   std::int32_t radialBlurSampleCount = 16;  ///< 放射ブラーのサンプル数。
   float radialBlurVisibleThreshold = 0.02f; ///< この演出量未満では RadialBlur を無効化する。

private:
   void ApplyNeutralEffect();

   float effectAmount_ = 0.0f; ///< 速度に応じて補間した画面演出の現在の適用量。
   float time_ = 0.0f;         ///< クリップ先頭からの時刻（秒）。
};

} // namespace App
