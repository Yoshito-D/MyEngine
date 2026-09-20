#pragma once
#include "RearCameraGravityUp.h"
#include "RearCameraTransition.h"
#include "RearCameraDirectionTracker.h"
#include "RearCameraPlanetGuide.h"
#include "RearCameraSpeedEffects.h"
#include "RearCameraAimSolver.h"

namespace App {

/// @brief 計測とUIへ渡す非所有の読み取り専用ビュー。更新呼び出しの間だけ使用する。
struct RearCameraFrameView {
   const RearCameraInput& input; ///< 外部入力
   const RearCameraTransition& transition; ///< 離着陸の遷移
   const RearCameraDirectionTracker& direction; ///< 重力Upと後方方向
   const RearCameraPlanetGuide& planet; ///< 惑星方向ガイド
   const RearCameraSpeedEffects& speed; ///< 速度によるFOVと距離の演出
   const RearCameraGravityUp& gravity; ///< 補間済み重力Up
   const RearCameraAimSolver& aim; ///< 注視点と最終姿勢
};

} // namespace App
