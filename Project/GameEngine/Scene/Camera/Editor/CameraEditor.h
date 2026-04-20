#pragma once

#ifdef USE_IMGUI

#include "../Core/CameraState.h"
#include "CameraGizmo.h"

namespace GameEngine {

class Camera;
class VirtualCamera;
class CinemachineBrain;
class LineRenderer;
class ICinemachineComponent;

/// @brief カメラエディタクラス
/// カメラのパラメータ編集、視錐台表示、VirtualCamera管理を行う
class CameraEditor {
public:
    CameraEditor() = default;
    ~CameraEditor() = default;

    /// @brief 初期化
    /// @param lineRenderer ギズモ描画用のラインレンダラー
    void Initialize(LineRenderer* lineRenderer);

    /// @brief メインエディタウィンドウを表示
    void ShowEditorWindow();

    /// @brief カメラ詳細パネルを表示
    /// @param camera 編集対象のカメラ
    void ShowCameraInspector(Camera* camera);

    /// @brief VirtualCamera詳細パネルを表示
    /// @param vcam 編集対象のVirtualCamera
    void ShowVirtualCameraInspector(VirtualCamera* vcam);

    /// @brief CinemachineBrain管理パネルを表示
    /// @param brain 管理対象のBrain
    void ShowBrainInspector(CinemachineBrain* brain);

    /// @brief ギズモを描画
    /// @param viewCamera ビューに使用するカメラ
    void DrawGizmos(Camera* viewCamera);

    /// @brief 編集対象のカメラを設定
    void SetTargetCamera(Camera* camera) { targetCamera_ = camera; }

    /// @brief 編集対象のVirtualCameraを設定
    void SetTargetVirtualCamera(VirtualCamera* vcam) { targetVirtualCamera_ = vcam; }

    /// @brief 編集対象のBrainを設定
    void SetTargetBrain(CinemachineBrain* brain) { targetBrain_ = brain; }

    /// @brief ギズモ設定を取得
    CameraGizmo::Settings& GetGizmoSettings() { return gizmo_.GetSettings(); }

    /// @brief ギズモを表示するかどうか
    void SetShowGizmo(bool show) { showGizmo_ = show; }
    bool GetShowGizmo() const { return showGizmo_; }

private:
    /// @brief Transform編集UI
    /// @param transform 編集対象
    /// @return 変更があった場合true
    bool EditTransform(Transform& transform);

    /// @brief 投影設定編集UI
    /// @param camera 編集対象
    /// @return 変更があった場合true
    bool EditProjectionSettings(Camera* camera);

    /// @brief CameraState編集UI
    /// @param state 編集対象
    /// @return 変更があった場合true
    bool EditCameraState(CameraState& state);

    /// @brief コンポーネント編集UI
    /// @param component 編集対象
    void EditComponent(ICinemachineComponent* component);

    /// @brief FollowBodyコンポーネントの編集
    void EditFollowBody(ICinemachineComponent* component);

    /// @brief OrbitalBodyコンポーネントの編集
    void EditOrbitalBody(ICinemachineComponent* component);

    /// @brief LookAtAimコンポーネントの編集
    void EditLookAtAim(ICinemachineComponent* component);

    /// @brief PerlinNoiseコンポーネントの編集
    void EditPerlinNoise(ICinemachineComponent* component);

    /// @brief ギズモ設定パネル
    void ShowGizmoSettings();

    CameraGizmo gizmo_;
    Camera* targetCamera_ = nullptr;
    VirtualCamera* targetVirtualCamera_ = nullptr;
    CinemachineBrain* targetBrain_ = nullptr;

    bool showGizmo_ = true;
    bool showGizmoSettings_ = false;
    int selectedVirtualCameraIndex_ = -1;
};

} // namespace GameEngine

#endif // USE_IMGUI
