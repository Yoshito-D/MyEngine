#pragma once

#ifdef USE_IMGUI

#include "Scene/Camera/Core/CameraState.h"

namespace GameEngine {

class Camera;
class VirtualCamera;
class LineRenderer;

/// @brief カメラギズモの描画クラス
/// 視錐台、カメラの向き、位置などを可視化する
class CameraGizmo {
public:
    /// @brief ギズモの表示設定
    struct Settings {
        bool showFrustum = true;       // 視錐台を表示
        bool showDirection = true;     // カメラの向きを表示
        bool showUpVector = true;     // 上方向ベクトルを表示
        bool showNearPlane = true;     // 近クリップ面を表示
        bool showFarPlane = true;      // 遠クリップ面を表示

        Vector4 frustumColor = { 1.0f, 1.0f, 0.0f, 1.0f };      // 黄色
        Vector4 nearPlaneColor = { 0.0f, 1.0f, 0.0f, 1.0f };    // 緑
        Vector4 farPlaneColor = { 1.0f, 0.0f, 0.0f, 1.0f };     // 赤
        Vector4 directionColor = { 0.0f, 0.5f, 1.0f, 1.0f };    // 青
        Vector4 upVectorColor = { 0.0f, 1.0f, 0.0f, 1.0f };     // 緑

        float frustumScale = 1.0f;     // 互換用。視錐台は実際のnear/farで描画する
    };

    CameraGizmo() = default;
    ~CameraGizmo() = default;

    /// @brief 初期化
    /// @param lineRenderer ラインレンダラー
    void Initialize(LineRenderer* lineRenderer);

    /// @brief Cameraの視錐台を描画
    /// @param camera 描画対象のカメラ
    /// @param viewCamera ビューに使用するカメラ
    void DrawFrustum(Camera* camera, Camera* viewCamera);

    /// @brief CameraStateの視錐台を描画
    /// @param state カメラ状態
    /// @param viewCamera ビューに使用するカメラ
    void DrawFrustum(const CameraState& state, Camera* viewCamera);

    /// @brief CameraStateの視錐台を指定アスペクト比で描画
    /// @param state カメラ状態
    /// @param viewCamera ビューに使用するカメラ
    /// @param aspectRatio 投影行列に使用するアスペクト比
    void DrawFrustum(const CameraState& state, Camera* viewCamera, float aspectRatio);

    /// @brief VirtualCameraの視錐台を描画
    /// @param vcam 仮想カメラ
    /// @param viewCamera ビューに使用するカメラ
    void DrawFrustum(VirtualCamera* vcam, Camera* viewCamera);

    /// @brief カメラアイコンを描画（簡易的なカメラ形状）
    /// @param position カメラ位置
    /// @param rotation カメラ回転
    /// @param scale スケール
    /// @param viewCamera ビューに使用するカメラ
    void DrawCameraIcon(const Vector3& position, const Quaternion& rotation, float scale, Camera* viewCamera);

    /// @brief 設定を取得
    Settings& GetSettings() { return settings_; }
    const Settings& GetSettings() const { return settings_; }

private:
    /// @brief ViewProjection行列から視錐台の8頂点を計算
    void CalculateFrustumCorners(
        const Matrix4x4& viewMatrix,
        const Matrix4x4& projectionMatrix,
        Vector3 outCorners[8]) const;

    Matrix4x4 MakeCameraProjectionMatrix(const Camera& camera, float farClip) const;
    Matrix4x4 MakeCameraStateProjectionMatrix(const CameraState& state, float aspectRatio, float farClip) const;
    Vector3 UnprojectClipPoint(const Vector4& clipPoint, const Matrix4x4& inverseViewProjection) const;
    void DrawOrientationVectors(const Matrix4x4& viewMatrix, Camera* viewCamera);

    /// @brief 四角形を描画
    void DrawQuad(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
                  const Vector4& color, Camera* viewCamera);

    LineRenderer* lineRenderer_ = nullptr;
    Settings settings_;
};

} // namespace GameEngine

#endif // USE_IMGUI
