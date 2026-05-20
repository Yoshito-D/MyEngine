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

   /// @brief 惑星切替に必要な距離差のヒステリシス（小さいほど敏感）
   float switchHysteresis = 1.5f;

   /// @brief 車体OBBの半サイズ（各軸を個別指定）
   /// Transformのscaleではなくこの値を距離判定に使用する
   GameEngine::Vector3 obbHalfExtents = { 0.5f, 0.5f, 0.5f };

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief パラメータをシリアライズする
   nlohmann::json Serialize() const override {
      nlohmann::json json;
      json["switchHysteresis"]    = switchHysteresis;
      json["obbHalfExtents"]["x"] = obbHalfExtents.x;
      json["obbHalfExtents"]["y"] = obbHalfExtents.y;
      json["obbHalfExtents"]["z"] = obbHalfExtents.z;
      return json;
   }

   /// @brief パラメータをデシリアライズする
   void Deserialize(const nlohmann::json& data) override {
      if (data.contains("switchHysteresis")) { switchHysteresis = data["switchHysteresis"]; }
      if (data.contains("obbHalfExtents")) {
         const auto& h = data["obbHalfExtents"];
         if (h.contains("x")) { obbHalfExtents.x = h["x"]; }
         if (h.contains("y")) { obbHalfExtents.y = h["y"]; }
         if (h.contains("z")) { obbHalfExtents.z = h["z"]; }
      }
   }

private:
   /// @brief 登録済み惑星候補
   std::vector<PlanetEntry> entries_;

   /// @brief 現在選択中インデックス
   int currentIndex_ = -1;
};

} // namespace App
