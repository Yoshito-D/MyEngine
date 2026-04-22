#include "PlayerController.h"
#include "Object/Component/TransformComponent.h"
#include "Object/Object.h"
#include "Framework/EngineContext.h"
#include "Utility/MathUtils/QuaternionOperations.h"
#include <cmath>
#include <algorithm>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

namespace GameEngine {

void PlayerController::Update(float deltaTime) {
    if (!HasOwner()) {
        return;
    }

    // GravityBodyから現在のUpVectorを取得
    auto* gravityBody = GetOwner().GetComponent<GravityBody>();
    Vector3 gravityUp = { 0.0f, 1.0f, 0.0f };  // デフォルト
    if (gravityBody) {
        gravityUp = gravityBody->GetCurrentUpVector();
    }

    // 入力を収集
    Vector2 input = CollectInput();

    // 入力がデッドゾーン以下なら何もしない
    float inputLength = std::sqrt(input.x * input.x + input.y * input.y);
    if (inputLength < inputDeadZone) {
        lastMoveDirection_ = { 0.0f, 0.0f, 0.0f };
        return;
    }

    // 入力を正規化（最大1.0にクランプ）
    float normalizedLength = std::min(inputLength, 1.0f);
    Vector2 normalizedInput = {
        (input.x / inputLength) * normalizedLength,
        (input.y / inputLength) * normalizedLength
    };

    // 直交基底を構築して移動方向を計算
    ApplyMovement(normalizedInput, gravityUp, deltaTime);
}

Vector2 PlayerController::CollectInput() const {
    Vector2 input = { 0.0f, 0.0f };

    // WASD入力
    if (EngineContext::IsKeyPressed(KeyCode::W)) { input.y += 1.0f; }
    if (EngineContext::IsKeyPressed(KeyCode::S)) { input.y -= 1.0f; }
    if (EngineContext::IsKeyPressed(KeyCode::A)) { input.x -= 1.0f; }
    if (EngineContext::IsKeyPressed(KeyCode::D)) { input.x += 1.0f; }

    // ゲームパッド左スティック（接続されていれば優先）
    Vector2 stick = EngineContext::GetLeftStick(0);
    if (std::abs(stick.x) > inputDeadZone || std::abs(stick.y) > inputDeadZone) {
        input = stick;
    }

    return input;
}

void PlayerController::ApplyMovement(const Vector2& input, const Vector3& gravityUp, float deltaTime) {
    auto* transform = GetOwner().GetComponent<TransformComponent>();
    if (!transform) {
        return;
    }

    // ========================================================
    // スクリーンスペース投影（Screen-Space Projection）
    //
    // カメラの「画面Up」と「画面Right」を重力平面に投影して
    // 移動基底を作る。こうすることで、プレイヤーが惑星のどこに
    // いても「スティック上 → 画面奥方向」が常に成立する。
    //
    // 手順:
    //   1. カメラの screenUp, screenRight を取得
    //   2. 各ベクトルを重力平面（gravityUp に垂直）へ投影
    //   3. 長さが 0 に近い場合はフォールバック（前フレームの値）
    //   4. moveDir = F_proj * inputY + R_proj * inputX
    // ========================================================

    // カメラの画面軸を取得
    // 優先度: GravityFollowCamera > OrbitalBody > Camera > ActiveCamera
    Vector3 screenUp    = { 0.0f, 1.0f, 0.0f };
    Vector3 screenRight = { 1.0f, 0.0f, 0.0f };
    if (gravityFollowCamera_) {
        screenUp    = gravityFollowCamera_->GetCameraUp();
        screenRight = gravityFollowCamera_->GetCameraRight();
    } else if (orbitalBody_) {
        screenUp    = orbitalBody_->GetCameraUp();
        screenRight = orbitalBody_->GetCameraRight();
    } else if (camera_) {
        Quaternion camQ = camera_->GetQuaternion();
        screenUp    = RotateVector({ 0.0f, 1.0f, 0.0f }, camQ);
        screenRight = RotateVector({ 1.0f, 0.0f, 0.0f }, camQ);
    } else if (auto* activeCamera = EngineContext::GetActiveCamera()) {
        Quaternion camQ = activeCamera->GetQuaternion();
        screenUp    = RotateVector({ 0.0f, 1.0f, 0.0f }, camQ);
        screenRight = RotateVector({ 1.0f, 0.0f, 0.0f }, camQ);
    }

    // screenUp を重力平面に投影 → F_proj（スティック上の前方向）
    auto projectOnPlane = [&](const Vector3& v) -> Vector3 {
        Vector3 proj = v - gravityUp * v.Dot(gravityUp);
        float len = proj.Length();
        return len > 1e-4f ? proj * (1.0f / len) : Vector3{ 0.0f, 0.0f, 0.0f };
    };

    Vector3 fProj = projectOnPlane(screenUp);
    if (fProj.LengthSquared() < 1e-6f) {
        fProj = lastForwardProj_;
    }

    // screenRight を重力平面に投影 → R_proj（スティック右の方向）
    Vector3 rProj = projectOnPlane(screenRight);
    if (rProj.LengthSquared() < 1e-6f) {
        rProj = lastRightProj_;
    }

    // デバッグ用にキャッシュ
    lastRightProj_   = rProj;
    lastForwardProj_ = fProj;

    // ========================================================
    // 2D入力 → 3D移動方向
    // ========================================================
    Vector3 moveDirection = fProj * input.y + rProj * input.x;

    if (moveDirection.LengthSquared() < 1e-8f) {
        lastMoveDirection_ = { 0.0f, 0.0f, 0.0f };
        return;
    }

    moveDirection = moveDirection.Normalize();
    lastMoveDirection_ = moveDirection;

    // ========================================================
    // 位置の更新
    // ========================================================
    Vector3& position = transform->transform.translation;
    position = position + moveDirection * moveSpeed * deltaTime;

    // ========================================================
    // キャラクターの向きを移動方向に合わせる
    //
    // GravityBodyは毎フレーム「currentUp→targetUp」の差分回転を
    // currentRotationに掛けてTransformを更新している。
    // PlayerControllerが LookRotation() で完全な新Quaternionを
    // 上書きすると GravityBody の傾き情報が消えてしまう。
    //
    // そのため、gravityUp軸周りのyaw回転だけを currentRotation に
    // 左掛けして加える。これにより：
    //   - GravityBodyが管理する「惑星上のup向き」はそのまま維持
    //   - PlayerControllerが「yaw方向だけ」補間して向きを変える
    // ========================================================
    Quaternion currentRotation = transform->transform.GetActiveQuaternion();

    // 現在の「forward」と目標moveDirのyaw角差をgravityUp軸周りで計算する
    // currentForward = currentRotation で (0,0,1) を回転した方向
    Vector3 currentForward = RotateVector({ 0.0f, 0.0f, 1.0f }, currentRotation);

    // currentForward と moveDirection を gravityUp 平面に投影
    Vector3 curFlatFwd = projectOnPlane(currentForward);
    Vector3 tgtFlatFwd = projectOnPlane(moveDirection);

    if (curFlatFwd.LengthSquared() > 1e-6f && tgtFlatFwd.LengthSquared() > 1e-6f) {
        // 2ベクトル間のyaw角度差を求める
        float cosA = std::clamp(curFlatFwd.Dot(tgtFlatFwd), -1.0f, 1.0f);
        float angle = std::acos(cosA);

        if (angle > 1e-4f) {
            // 符号をgravityUp軸の外積で判定
            float sign = gravityUp.Dot(curFlatFwd.Cross(tgtFlatFwd)) >= 0.0f ? 1.0f : -1.0f;
            float step = std::clamp(turnSpeed * deltaTime, 0.0f, angle);
            Quaternion yawDelta = MakeRotateAxisAngleQuaternion(gravityUp, sign * step);
            // gravityUp軸yaw回転をcurrentRotationに左掛けする
            Quaternion newRotation = (yawDelta * currentRotation).Normalize();
            transform->transform.SetRotationQuaternion(newRotation);
        }
    }
}

#ifdef USE_IMGUI
void PlayerController::DrawInspector() {
    ImGui::Text("PlayerController Component");
    ImGui::Separator();

    ImGui::DragFloat("Move Speed",    &moveSpeed,    0.1f, 0.0f, 50.0f);
    ImGui::DragFloat("Turn Speed",    &turnSpeed,    0.5f, 0.0f, 30.0f);
    ImGui::DragFloat("Input DeadZone",&inputDeadZone,0.01f,0.0f,  1.0f);

    ImGui::Spacing();
    ImGui::Text("Move Direction: (%.2f, %.2f, %.2f)",
        lastMoveDirection_.x, lastMoveDirection_.y, lastMoveDirection_.z);
    ImGui::Text("F_proj: (%.2f, %.2f, %.2f)",
        lastForwardProj_.x, lastForwardProj_.y, lastForwardProj_.z);
    ImGui::Text("R_proj: (%.2f, %.2f, %.2f)",
        lastRightProj_.x, lastRightProj_.y, lastRightProj_.z);
}
#endif

nlohmann::json PlayerController::Serialize() const {
    nlohmann::json json;
    json["moveSpeed"]     = moveSpeed;
    json["turnSpeed"]     = turnSpeed;
    json["inputDeadZone"] = inputDeadZone;
    return json;
}

void PlayerController::Deserialize(const nlohmann::json& data) {
    if (data.contains("moveSpeed"))     { moveSpeed     = data["moveSpeed"]; }
    if (data.contains("turnSpeed"))     { turnSpeed     = data["turnSpeed"]; }
    if (data.contains("inputDeadZone")) { inputDeadZone = data["inputDeadZone"]; }
}

} // namespace GameEngine
