#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "GravityAttractor.h"
#include <vector>

namespace App {

/// @brief プレイヤー位置に応じて有効惑星を選択し、関連コンポーネントを切り替える
class PlanetSwitcher final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "PlanetSwitcher";

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 近傍惑星を選び、リンク先を更新する
   void Update(float deltaTime) override;

   /// @brief 惑星1件分の切替情報
   struct PlanetEntry {
      GravityAttractor*   attractor    = nullptr; ///< 重力発生源
      GameEngine::Vector3 center       = {};      ///< 惑星中心座標
      float               surfaceRadius = 15.0f;  ///< 地表半径
   };

   /// @brief 切替候補惑星を登録する
   void AddPlanet(GravityAttractor* attractor,
                  const GameEngine::Vector3& center,
                  float surfaceRadius) {
      entries_.push_back({ attractor, center, surfaceRadius });
   }

   /// @brief 現在選択中の惑星インデックスを返す
   int GetCurrentPlanetIndex() const { return currentIndex_; }

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief シリアライズ（動的参照のため保存項目なし）
   nlohmann::json Serialize() const override  { return {}; }

   /// @brief デシリアライズ（保存項目なしのため処理なし）
   void Deserialize(const nlohmann::json&) override {}

private:
   /// @brief 登録済み惑星候補
   std::vector<PlanetEntry> entries_;

   /// @brief 現在選択中インデックス
   int currentIndex_ = -1;
};

} // namespace App
