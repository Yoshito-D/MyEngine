#include "GravityBody.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace GameEngine {

void GravityBody::Update(float deltaTime) {
    if (!HasOwner()) {
        return;
    }

    // 姿勢の更新（重力方向に向けて回転）
    UpdateRotation(deltaTime);

    // 物理演算（重力加速度を適用）
    if (useGravity) {
        UpdatePhysics(deltaTime);
    }
}

void GravityBody::SetTargetUpVector(const Vector3& targetUp) {
    // エンジン側でNormalize()が0ベクトルを処理するようになったので簡略化
    targetUpVector_ = targetUp.Normalize();
}

void GravityBody::SetGravity(const Vector3& gravity) {
    gravityAcceleration_ = gravity;
}

void GravityBody::UpdateRotation(float deltaTime) {
    auto* transform = GetOwner().GetComponent<TransformComponent>();
    if (!transform) {
        return;
    }

    // 正規化（エンジン側で0ベクトルは安全に処理される）
    Vector3 current = currentUpVector_.Normalize();
    Vector3 target = targetUpVector_.Normalize();

    // 0ベクトルの場合はデフォルトに戻す
    if (current.LengthSquared() < 1e-8f) {
        current = Vector3{ 0.0f, 1.0f, 0.0f };
        currentUpVector_ = current;
    }
    if (target.LengthSquared() < 1e-8f) {
        target = Vector3{ 0.0f, 1.0f, 0.0f };
        targetUpVector_ = target;
    }

    // ほぼ同じ方向なら何もしない
    float dot = current.Dot(target);
    if (dot > 0.9999f) {
        return;
    }

    // 逆方向の場合の処理
    if (dot < -0.9999f) {
        // 任意の垂直軸を見つける
        Vector3 axis = Vector3{ 1.0f, 0.0f, 0.0f }.Cross(current);
        if (axis.LengthSquared() < 1e-6f) {
            axis = Vector3{ 0.0f, 1.0f, 0.0f }.Cross(current);
        }
        if (axis.LengthSquared() < 1e-6f) {
            axis = Vector3{ 0.0f, 0.0f, 1.0f }.Cross(current);
        }

        // 軸が見つからない場合は処理をスキップ
        if (axis.LengthSquared() < 1e-6f) {
            return;
        }

        axis = axis.Normalize();

        // 180度回転のクォータニオンを生成
        Quaternion rotationDelta = MakeRotateAxisAngleQuaternion(axis, 3.14159265358979323846f);
        Quaternion currentRotation = transform->transform.GetActiveQuaternion();
        Quaternion newRotation = (rotationDelta * currentRotation).Normalize();

        transform->transform.SetRotationQuaternion(newRotation);
        currentUpVector_ = target;
        return;
    }

    // 回転軸を計算（外積）
    Vector3 rotationAxis = current.Cross(target);

    // 外積が0ベクトルの場合は処理をスキップ
    if (rotationAxis.LengthSquared() < 1e-6f) {
        return;
    }

    rotationAxis = rotationAxis.Normalize();

    // 回転角を計算
    float angle = std::acos(std::clamp(dot, -1.0f, 1.0f));

    // 角度が極小の場合もスキップ
    if (std::abs(angle) < 1e-6f) {
        return;
    }

    // Slerpの補間係数を計算
    float t = std::clamp(rotationSpeed * deltaTime, 0.0f, 1.0f);

    // 回転クォータニオンを生成
    Quaternion rotationDelta = MakeRotateAxisAngleQuaternion(rotationAxis, angle * t);

    // 現在の回転に適用（エンジン側のNormalize()が安全に処理）
    Quaternion currentRotation = transform->transform.GetActiveQuaternion();
    Quaternion newRotation = (rotationDelta * currentRotation).Normalize();

    // Transformに反映
    transform->transform.SetRotationQuaternion(newRotation);

    // 現在のUpVectorを更新（Lerpで安全に補間）
    if (t >= 0.9999f) {
        currentUpVector_ = target;
    } else {
        currentUpVector_ = Vector3::Lerp(current, target, t);
    }
}

void GravityBody::UpdatePhysics(float deltaTime) {
    auto* transform = GetOwner().GetComponent<TransformComponent>();
    if (!transform) {
        return;
    }

    // 速度に重力加速度を加算（オイラー積分）
    velocity_ += gravityAcceleration_ * deltaTime;

    // 位置を更新
    Vector3 newPosition = transform->transform.translation + velocity_ * deltaTime;
    transform->transform.translation = newPosition;
}

#ifdef USE_IMGUI
void GravityBody::DrawInspector() {
    ImGui::Text("GravityBody Component");
    ImGui::Separator();

    ImGui::Checkbox("Use Gravity", &useGravity);
    ImGui::DragFloat("Rotation Speed", &rotationSpeed, 0.1f, 0.1f, 20.0f);
    ImGui::DragFloat("Gravity Strength", &gravityStrength, 0.1f, 0.0f, 50.0f);

    ImGui::Spacing();
    ImGui::Text("Current Up Vector: (%.2f, %.2f, %.2f)", 
        currentUpVector_.x, currentUpVector_.y, currentUpVector_.z);
    ImGui::Text("Target Up Vector: (%.2f, %.2f, %.2f)", 
        targetUpVector_.x, targetUpVector_.y, targetUpVector_.z);
    ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", 
        velocity_.x, velocity_.y, velocity_.z);
}
#endif

nlohmann::json GravityBody::Serialize() const {
    nlohmann::json json;
    json["rotationSpeed"] = rotationSpeed;
    json["gravityStrength"] = gravityStrength;
    json["useGravity"] = useGravity;
    json["currentUpVector"] = { currentUpVector_.x, currentUpVector_.y, currentUpVector_.z };
    json["velocity"] = { velocity_.x, velocity_.y, velocity_.z };
    return json;
}

void GravityBody::Deserialize(const nlohmann::json& data) {
    if (data.contains("rotationSpeed")) {
        rotationSpeed = data["rotationSpeed"];
    }
    if (data.contains("gravityStrength")) {
        gravityStrength = data["gravityStrength"];
    }
    if (data.contains("useGravity")) {
        useGravity = data["useGravity"];
    }
    if (data.contains("currentUpVector")) {
        auto up = data["currentUpVector"];
        currentUpVector_ = { up[0], up[1], up[2] };
        targetUpVector_ = currentUpVector_;
    }
    if (data.contains("velocity")) {
        auto vel = data["velocity"];
        velocity_ = { vel[0], vel[1], vel[2] };
    }
}

} // namespace GameEngine
