#pragma once
#include "Utility/Math/Quaternion.h"
#include "Utility/Math/Vector3.h"

class Planet;
class SphericalMovementComponent;

/// @brief 惑星間の切り替えを管理するヘルパー
class PlanetSwitchHelper {
public:
   /// @brief 惑星を切り替えるべきかチェック
   /// @param currentPlanet 現在の惑星
   /// @param newPlanet 新しい惑星候補
   /// @param currentPosition 現在の位置
   /// @param switchThreshold 切り替えのしきい値
   /// @return 切り替えるべきならtrue
   static bool ShouldSwitchPlanet(
      Planet* currentPlanet,
      Planet* newPlanet,
      const GameEngine::Vector3& currentPosition,
      float switchThreshold = 0.5f
   );

   /// @brief 新しい惑星に対して球面移動コンポーネントを再初期化
   /// @param sphericalMovement 球面移動コンポーネント
   /// @param newPlanet 新しい惑星
   /// @param currentPosition 現在の位置
   /// @param currentForwardDirection 現在の前方向（保持したい向き）
   static void SwitchToPlanet(
      SphericalMovementComponent* sphericalMovement,
      Planet* newPlanet,
      const GameEngine::Vector3& currentPosition,
      const GameEngine::Vector3& currentForwardDirection
   );
};
