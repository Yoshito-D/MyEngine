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

   /// @brief 保存データとコンポーネント検索に使う型名を返す。
   /// @return 「PlanetDistanceColorComponent」を表す固定文字列。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief プレイヤーと惑星地表との距離から、マテリアルのベース色をグレーへ補間する。
   /// @param deltaTime 未使用。距離（m）からそのフレームの色を直接求める。
   /// @details プレイヤー参照がない、または距離が非有限の場合は適用前の色へ戻す。
   void Update(float deltaTime) override;

   /// @brief 距離演出で変更したマテリアルの色を戻し、保持していた色の履歴を破棄する。
   /// @details 演出後に外部で差し替え・再編集された色は上書きしない。
   void OnDisable() override;

   /// @brief デタッチ前に距離演出による色変更を解除し、色の履歴を破棄する。
   /// @details 差し替え済みマテリアルや外部で再編集された色は保持する。
   void OnDetach() override;

   /// @brief 惑星の距離連動色の保存対象設定をJSONへ変換する。
   /// @return 距離の基準となるプレイヤーIDと色の減衰距離を含む設定JSON。
   nlohmann::json Serialize() const override;

   /// @brief 保存JSONから惑星の距離連動色の設定を復元する。
   /// @param data 距離の基準となるプレイヤーIDと色の減衰距離を含む設定JSON。
   void Deserialize(const nlohmann::json& data) override;

   /// @brief 惑星の距離連動色の共有可能な調整値をJSONで取得する。
   /// @return プレイヤーIDを除いた色の減衰距離（m）を含む共有設定JSON。
   nlohmann::json GetSharedParameters() const override;

   /// @brief 惑星の距離連動色の共有設定を既存コンポーネントへ反映する。
   /// @param data プレイヤーIDを除いた色の減衰距離（m）を含む共有設定JSON。
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief 惑星の距離連動色の距離の基準となるプレイヤーIDと色の減衰距離をインスペクターで表示・編集する。
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
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

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
