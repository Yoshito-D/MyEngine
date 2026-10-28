#pragma once

#include <cmath>
#include <algorithm>

#include "GameProject/App/Component/Gravity/GravityAttractor.h"

namespace App {

/// @brief 中心から放射状に重力Upを返す球状重力発生源
class SphericalGravityAttractor final : public GravityAttractor {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "SphericalGravityAttractor";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{
      "球状重力アトラクター", "Spherical Gravity Attractor"};

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 影響半径内かどうかを返す（0以下は無限範囲）
   bool IsInRange(const GameEngine::Vector3& objectPosition) const override;

   /// @brief 中心から対象への方向を重力Upとして返す
   GameEngine::Vector3 GetUpVectorFor(const GameEngine::Vector3& objectPosition) const override;

   /// @brief influenceRadius をシリアライズする
   nlohmann::json Serialize() const override;

   /// @brief influenceRadius をデシリアライズする
   void Deserialize(const nlohmann::json& data) override;

   /// @copydoc GameEngine::ParameterComponent::GetSharedParameters
   nlohmann::json GetSharedParameters() const override;

   /// @copydoc GameEngine::ParameterComponent::ApplySharedParameters
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings {

      /// @brief 影響半径（0以下で無限）
      float influenceRadius = 0.0f;
   };

   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const;

   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested);

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   float influenceRadius = 0.0f; ///< 影響半径（0以下で無限）。

private:
   /// @brief 発生源中心座標（オーナーTransform）
   GameEngine::Vector3 GetCenter() const;
};

} // namespace App
