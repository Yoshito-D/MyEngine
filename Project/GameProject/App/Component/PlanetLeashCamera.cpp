#include "pch.h"
#include "PlanetLeashCamera.h"
#include "Scene/Camera/Core/CameraState.h"
#include "Utility/MathUtils/MatrixOperations.h"
#include <cmath>
#include <algorithm>

namespace GameEngine {

void PlanetLeashCamera::MutateCameraState(CameraState& state, float deltaTime) {
    // --- 初回は現在のtransformをeyePos_の初期値として使う ---
    if (!isInitialized_) {
        eyePos_        = state.transform.translation;
        prevGravityUp_ = gravityUp_;
        eyeRelUp_      = gravityUp_;
        isInitialized_ = true;
    }

    // ================================================================
    // 0. gravityUp の変化分だけ eyePos_ と eyeRelUp_ を pivotTarget_ 中心に回転
    //
    // prevGravityUp_ → gravityUp_ への最短回転（Rodrigues）を適用。
    // eyePos_  : 重力フレームに対してカメラが同じ相対位置を保つ（ロール防止）
    // eyeRelUp_: LookAt の up に使うカメラのローカルUp。
    //            gravityUp_ を直接 up に使うと 180° 反転時にカメラが裏返るため、
    //            こちらを差分回転で連続更新してその問題を回避する。
    // ================================================================
    {
        Vector3 up0 = prevGravityUp_;
        Vector3 up1 = gravityUp_;
        float u0Len = up0.Length(), u1Len = up1.Length();
        if (u0Len > 1e-6f && u1Len > 1e-6f) {
            up0 = up0 * (1.0f / u0Len);
            up1 = up1 * (1.0f / u1Len);
            float cosA = std::clamp(up0.Dot(up1), -1.0f, 1.0f);
            if (cosA < 1.0f - 1e-7f) {
                Vector3 axis = up0.Cross(up1);
                float axLen = axis.Length();
                if (axLen > 1e-6f) {
                    axis = axis * (1.0f / axLen);
                    float angle = std::acos(cosA);
                    float c = std::cos(angle), s = std::sin(angle);
                    // Rodrigues ヘルパーラムダ
                    auto rodrigues = [&](const Vector3& v) -> Vector3 {
                        return v * c + axis.Cross(v) * s + axis * (axis.Dot(v) * (1.0f - c));
                    };
                    // eyePos_ を pivotTarget_ 中心に回転
                    Vector3 r = eyePos_ - pivotTarget_;
                    eyePos_ = pivotTarget_ + rodrigues(r);
                    // eyeRelUp_ も同じ回転で更新（LookAt反転防止）
                    eyeRelUp_ = rodrigues(eyeRelUp_);
                    float upNLen = eyeRelUp_.Length();
                    if (upNLen > 1e-6f) eyeRelUp_ = eyeRelUp_ * (1.0f / upNLen);
                }
            }
        }
        prevGravityUp_ = gravityUp_;
    }

    // ================================================================
    // 1. レアッシュ: プレイヤーから maxFollowDistance を超えていたら
    //    カメラを followSpeed で近づける
    // ================================================================
    Vector3 toTarget = pivotTarget_ - eyePos_;
    float dist = toTarget.Length();

    if (dist > maxFollowDistance) {
        float over  = dist - maxFollowDistance;
        float move  = (std::min)(over, followSpeed * deltaTime);
        Vector3 dir = toTarget * (1.0f / dist);
        eyePos_ = eyePos_ + dir * move;
    }

    // ================================================================
    // 2. 惑星クランプ: sphereCenter_ から minPlanetDistance より
    //    近づかないよう eye を押し出す
    // ================================================================
    Vector3 fromCenter = eyePos_ - sphereCenter_;
    float   fromCenterDist = fromCenter.Length();
    if (fromCenterDist < minPlanetDistance && fromCenterDist > 1e-6f) {
        Vector3 pushDir = fromCenter * (1.0f / fromCenterDist);
        eyePos_ = sphereCenter_ + pushDir * minPlanetDistance;
    }

    // ================================================================
    // 3. LookAt: eyePos_ から pivotTarget_ を注視する
    //    upベクトルは useGravityUp が true なら gravityUp_ を使う
    // ================================================================
    Vector3 lookDir = pivotTarget_ - eyePos_;
    float lookLen = lookDir.Length();
    if (lookLen < 1e-6f) {
        return;
    }
    Vector3 lookDirN = lookDir * (1.0f / lookLen);

    // ================================================================
    // eyeRelUp_ を lookDirN に垂直な平面へ再投影して直交を保証する
    //
    // レアッシュ移動・惑星クランプで eyePos_ が動くと lookDirN が変化する。
    // eyeRelUp_ がその新しい lookDirN に対して斜めになっていると
    // LookAt の xaxis = upVec × zaxis がロール（Z軸回転）を生む。
    // 投影することで常に upVec ⊥ lookDirN を保証し、ロールを消す。
    // ================================================================
    {
        Vector3 projected = eyeRelUp_ - lookDirN * eyeRelUp_.Dot(lookDirN);
        float pLen = projected.Length();
        if (pLen > 1e-6f) {
            eyeRelUp_ = projected * (1.0f / pLen);
        } else {
            // 完全縮退（eyeRelUp_ が lookDirN と完全平行）: 任意の直交ベクトルを生成
            Vector3 tmp = (std::abs(lookDirN.x) < 0.9f) ? Vector3{ 1.0f, 0.0f, 0.0f }
                                                         : Vector3{ 0.0f, 1.0f, 0.0f };
            tmp = tmp - lookDirN * tmp.Dot(lookDirN);
            eyeRelUp_ = tmp * (1.0f / tmp.Length());
        }
    }
    Vector3 upVec = eyeRelUp_;

    // LookAt と同じ計算でスクリーン軸をキャッシュ
    // zaxis = normalize(target - eye)
    // xaxis = normalize(upVec × zaxis)   ← 左手系
    // yaxis = zaxis × xaxis
    Vector3 zaxis = lookDirN;
    Vector3 xaxis = upVec.Cross(zaxis);
    float xLen = xaxis.Length();
    if (xLen > 1e-6f) {
        cachedRight_ = xaxis * (1.0f / xLen);
        cachedUp_    = zaxis.Cross(cachedRight_);
    }

    state.transform.translation = eyePos_;
    state.SetViewMatrix(MakeLookAtMatrix(eyePos_, pivotTarget_, upVec));
}

} // namespace GameEngine
