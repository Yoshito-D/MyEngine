#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "Utility/Math/Vector2.h"
#include "Scene/Camera/Camera.h"
#include "Scene/Camera/Components/OrbitalBody.h"
#include "GravityFollowCamera.h"
#include "GravityBody.h"

namespace GameEngine {

/// @brief カメラ相対の重力平面移動コンポーネント（フェーズ3）
///
/// カメラの前方ベクトルを現在の重力法線（UpVector）の平面に投影し、
/// 直交基底（F_proj, R_proj）を構築したうえでプレイヤー入力を変換する。
/// ジンバルロックが発生しないようにオイラー角を一切使用しない。
///
/// アルゴリズム概要:
///   1. カメラの Forward ベクトルを取得する
///   2. R_proj = normalize(cameraForward × gravityUp)   // 重力平面上の右ベクトル
///   3. F_proj = normalize(gravityUp × R_proj)           // 重力平面上の前ベクトル
///   4. moveDir = F_proj * inputY + R_proj * inputX      // 最終移動方向
class PlayerController final : public IObjectComponent {
public:
    static constexpr const char* kTypeName = "PlayerController";
    const char* GetTypeName() const override { return kTypeName; }

    void Update(float deltaTime) override;

    /// @brief 参照するカメラを設定する
    /// @param camera 参照カメラ（nullptrで無効化）
    void SetCamera(Camera* camera) { camera_ = camera; }

    /// @brief OrbitalBodyを設定する（カメラ相対移動で正確なForwardを取得するために使用）
    void SetOrbitalBody(OrbitalBody* orbitalBody) { orbitalBody_ = orbitalBody; }

    /// @brief GravityFollowCameraを設定する（フェーズ4: 重力追従カメラからスクリーン軸を取得）
    void SetGravityFollowCamera(GravityFollowCamera* cam) { gravityFollowCamera_ = cam; }

    /// @brief 最後に計算された移動方向ベクトルを取得する（デバッグ用）
    Vector3 GetLastMoveDirection() const { return lastMoveDirection_; }

    /// @brief 最後に計算されたR_projを取得する（デバッグ用）
    Vector3 GetLastRightProjected() const { return lastRightProj_; }

    /// @brief 最後に計算されたF_projを取得する（デバッグ用）
    Vector3 GetLastForwardProjected() const { return lastForwardProj_; }

#ifdef USE_IMGUI
    void DrawInspector() override;
#endif

    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& data) override;

public:
    float moveSpeed   = 5.0f;     ///< 移動速度（ワールド単位/秒）
    float turnSpeed   = 10.0f;    ///< 振り向き速度（Slerpのt倍率）
    float inputDeadZone = 0.1f;   ///< 入力デッドゾーン（これ未満の入力は無視）

private:
    /// @brief WASD + ゲームパッド左スティックから2D入力を収集する
    /// @return 正規化前の入力ベクトル（X: 横, Y: 前後）
    Vector2 CollectInput() const;

    /// @brief カメラのForwardと重力UpVectorから直交基底を構築して移動方向を計算する
    ///
    /// @param input        2D入力ベクトル（X: 横, Y: 前後）
    /// @param gravityUp    現在の重力Up方向（GravityBodyのCurrentUpVector）
    /// @param deltaTime    フレーム時間
    void ApplyMovement(const Vector2& input, const Vector3& gravityUp, float deltaTime);

private:
    Camera* camera_ = nullptr;                          ///< 参照カメラ
    OrbitalBody* orbitalBody_ = nullptr;             ///< OrbitalBody（GetCameraForward用）
    GravityFollowCamera* gravityFollowCamera_ = nullptr; ///< GravityFollowCamera（フェーズ4）

    // デバッグ用キャッシュ
    Vector3 lastMoveDirection_ = { 0.0f, 0.0f, 0.0f };
    Vector3 lastRightProj_    = { 1.0f, 0.0f, 0.0f };
    Vector3 lastForwardProj_  = { 0.0f, 0.0f, 1.0f };
};

} // namespace GameEngine
