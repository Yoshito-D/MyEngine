#include "GravityAttractor.h"

/// @brief GravityBody に重力方向と加速度を適用する
bool App::GravityAttractor::ApplyTo(GravityBody& gravityBody, const GameEngine::Vector3& objectPosition) const {
    // 非有効時は何もしない
    if (!IsEnabled()) { return false; }

    // 影響範囲外なら適用しない
    if (!IsInRange(objectPosition)) { return false; }

    // Up方向を基準に姿勢目標と重力加速度を設定
    GameEngine::Vector3 upVector = GetUpVectorFor(objectPosition);
    if (upVector.LengthSquared() < 1e-8f) { return false; }
    upVector = upVector.Normalize();
    gravityBody.SetTargetUpVector(upVector);
    gravityBody.SetGravity(-upVector * gravityBody.gravityStrength);
    return true;
}
