#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"
#include "Utility/Math/Vector3.h"
#include "GravityBody.h"
#include <vector>

namespace GameEngine {

/// @brief 重力発生源の抽象基底クラス
/// 指定した領域内のGravityBodyに対して重力ベクトルを提供する
class GravityAttractor : public IObjectComponent {
public:
    /// @brief 影響範囲内にあるか判定する
    /// @param objectPosition 対象オブジェクトの座標
    /// @return 影響範囲内であればtrue
    virtual bool IsInRange(const Vector3& objectPosition) const = 0;

    /// @brief 指定した座標に対するUpVectorを計算して返す（純粋仮想）
    /// @param objectPosition 対象オブジェクトの座標
    /// @return 重力に対するUpVector（正規化済み）
    virtual Vector3 GetUpVectorFor(const Vector3& objectPosition) const = 0;

    /// @brief GravityBodyに重力を適用する
    /// @param gravityBody 対象のGravityBody
    /// @param objectPosition 対象オブジェクトの座標
    void ApplyTo(GravityBody& gravityBody, const Vector3& objectPosition) const {
        if (!IsEnabled()) {
            return;
        }
        if (!IsInRange(objectPosition)) {
            return;
        }

        // 対象のUpVectorを計算して設定
        Vector3 upVector = GetUpVectorFor(objectPosition);
        gravityBody.SetTargetUpVector(upVector);

        // 重力加速度ベクトルを設定（Upと逆方向に重力）
        Vector3 gravityDir = -upVector;
        gravityBody.SetGravity(gravityDir * gravityBody.gravityStrength);
    }

public:
    bool enabled = true;  ///< 重力発生源の有効フラグ
};

} // namespace GameEngine
