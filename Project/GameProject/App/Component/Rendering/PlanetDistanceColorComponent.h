#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector4.h"
#include <string>
#include <vector>

namespace GameEngine {
class Material;
}

namespace App {

/// @brief 球状惑星の地表からプレイヤーまでの距離に応じて、専用マテリアルのベース色をグレーへ近づける。
class PlanetDistanceColorComponent final : public GameEngine::IObjectComponent {
public:
   /// @brief シーン保存とファクトリー検索で使うコンポーネント型名。
   static constexpr const char* kTypeName = "PlanetDistanceColorComponent";

   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"惑星距離ベースカラー", "Planet Distance Base Color"};

   /// @copydoc GameEngine::IObjectComponent::GetTypeName
   const char* GetTypeName() const override { return kTypeName; }

   /// @copydoc GameEngine::IObjectComponent::Update
   void Update(float deltaTime) override;

   /// @copydoc GameEngine::IObjectComponent::OnDisable
   void OnDisable() override;

   /// @copydoc GameEngine::IObjectComponent::OnDetach
   void OnDetach() override;

   /// @copydoc GameEngine::IObjectComponent::Serialize
   nlohmann::json Serialize() const override;

   /// @copydoc GameEngine::IObjectComponent::Deserialize
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @copydoc GameEngine::IObjectComponent::DrawInspector
   void DrawInspector() override;
#endif

   /// @brief 保存・編集するプレイヤー参照と距離設定。距離はワールド単位。
   struct Settings {
      std::string playerObjectId;      ///< 距離の基準となるプレイヤーの安定Entity ID。
      float fadeStartDistance = 10.0f; ///< 元のベース色を維持する地表からの距離。
      float fadeEndDistance = 300.0f;  ///< ベース色がグレーになる地表からの距離。
   };

   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const { return settings_; }

   /// @brief 距離を有限かつ0以上・開始より終了が大きい範囲へ検証して設定する。
   void Configure(const Settings& requested);

private:
   struct MaterialColorState {
      const GameEngine::Material* material = nullptr; ///< 比較だけに使う、スロットの変更検知用参照。
      GameEngine::Vector4 baseColor{};                ///< 距離による補間をかける前の設定色。
      GameEngine::Vector4 appliedColor{};             ///< 外部からの色変更を検知する、直近の適用色。
   };

   void ApplyGrayBlend(float amount);

   void ResetBaseColor();

   Settings settings_;                           ///< 保存設定。プレイヤーの生ポインターは保持しない。
   std::vector<MaterialColorState> colorStates_; ///< 各スロットの元の色と実行中の適用状態。
};

} // namespace App
