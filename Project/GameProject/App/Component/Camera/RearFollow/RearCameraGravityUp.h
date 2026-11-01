#pragma once

#include <cstdint>
#include "GameProject/App/Component/Camera/RearFollow/RearCameraTypes.h"

namespace App {

/// @brief 惑星切替と着地予測に対して重力Upを連続的に追従させる。
class RearCameraGravityUp {
public:
   /// @brief 補間済みUpを初期方向へ戻す。
   void Reset(const RearCameraSettings& settings);

   /// @brief 補間済み重力Upを読み取り専用で返す。
   const RearCameraGravityState& GetState() const { return state_; }

   /// @brief 入力と前フレームの軸から、角速度を制限した重力Upを計算する。
   GameEngine::Vector3 SmoothGravityUp(const RearCameraInput& input, const RearCameraSettings& settings,
      const RearCameraTransitionState& transition, const RearCameraAimState& aim, float deltaTime);

private:
   RearCameraGravityState state_; ///< 補間済み重力Up。
};

} // namespace App
