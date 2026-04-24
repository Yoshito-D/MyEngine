#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "GravityAttractor.h"
#include <vector>

namespace App {

/// @brief 複数惑星の中から最も近い（影響の強い）惑星を毎フレーム選択し、
///        同オーナーの GravityAttractorLink / CharacterLanding / CameraGravityBridge
///        の参照先を自動的に切り替えるコンポーネント
///
/// 使い方:
///   1. AddPlanet() で惑星情報を登録
///   2. player にアタッチするだけで自動切り替えが動作する
class PlanetSwitcher final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "PlanetSwitcher";
   const char* GetTypeName() const override { return kTypeName; }

   void Update(float deltaTime) override;

   struct PlanetEntry {
      GravityAttractor*   attractor    = nullptr; ///< 重力発生源
      GameEngine::Vector3 center       = {};       ///< 惑星中心座標
      float               surfaceRadius = 15.0f;  ///< プレイヤーが乗る表面半径
   };

   /// @brief 惑星を登録する
   void AddPlanet(GravityAttractor* attractor,
                  const GameEngine::Vector3& center,
                  float surfaceRadius) {
      entries_.push_back({ attractor, center, surfaceRadius });
   }

   /// @brief 現在乗っている惑星インデックスを取得（デバッグ用）
   int GetCurrentPlanetIndex() const { return currentIndex_; }

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   nlohmann::json Serialize() const override  { return {}; }
   void Deserialize(const nlohmann::json&) override {}

private:
   std::vector<PlanetEntry> entries_;
   int currentIndex_ = -1;
};

} // namespace App
