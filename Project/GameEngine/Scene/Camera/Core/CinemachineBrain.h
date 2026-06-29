#pragma once
#include "CameraState.h"
#include "VirtualCamera.h"
#include <vector>
#include <stack>
#include <memory>
#include <climits>

namespace GameEngine {

class Camera;

/// @brief Cinemachineのメイン制御クラス
/// 複数のVirtualCameraを管理し、最終的なカメラ状態を計算
class CinemachineBrain {
public:
    CinemachineBrain() = default;
    ~CinemachineBrain() = default;

    /// @brief 初期化（Cameraの所有権を受け取る）
    /// @param outputCamera 出力先カメラ（所有権移譲）
    void Initialize(std::unique_ptr<Camera> outputCamera);

    /// @brief 出力カメラを取得
    Camera* GetOutputCamera() const { return outputCamera_.get(); }

    /// @brief 更新処理
    /// @param deltaTime フレーム時間
    void Update(float deltaTime);

    /// @brief エディタ停止中のプレビュー更新
    /// @param deltaTime フレーム時間
    /// @param editorDrivenCamera 入力操作だけを反映するエディタ用VirtualCamera
    void UpdateEditorPreview(float deltaTime, VirtualCamera* editorDrivenCamera);

    /// @brief VirtualCameraを登録
    void RegisterVirtualCamera(VirtualCamera* vcam);

    /// @brief VirtualCameraの登録解除
    void UnregisterVirtualCamera(VirtualCamera* vcam);

    /// @brief 現在のカメラ状態を取得
    const CameraState& GetCurrentState() const { return currentState_; }

    /// @brief アクティブなVirtualCameraを取得
    VirtualCamera* GetActiveCamera() const { return activeCamera_; }

    /// @brief デフォルトのブレンド時間を設定
    void SetDefaultBlendTime(float seconds) { defaultBlendTime_ = seconds; }
    float GetDefaultBlendTime() const { return defaultBlendTime_; }

    /// @brief 即座にカメラを切り替え（ブレンドなし）
    void Cut(VirtualCamera* vcam);

    /// @brief 登録されているVirtualCameraの数を取得
    size_t GetVirtualCameraCount() const { return virtualCameras_.size(); }

    /// @brief 登録されているVirtualCamera一覧を取得
    const std::vector<VirtualCamera*>& GetVirtualCameras() const { return virtualCameras_; }

private:
    /// @brief ブレンド中の1層を表す構造体
    struct BlendLayer {
        CameraState fromState;   // ブレンド開始時の状態
        VirtualCamera* toCamera; // ブレンド先カメラ
        float duration;          // ブレンド時間
        float progress;          // 進行度 [0, 1]
    };

    VirtualCamera* FindHighestPriorityCamera() const;
    void BlendToCamera(VirtualCamera* newCamera);
    void UpdateBlend(float deltaTime);
    void ApplyStateToOutputCamera();

    std::vector<VirtualCamera*> virtualCameras_;
    std::unique_ptr<Camera> outputCamera_ = nullptr;

    VirtualCamera* activeCamera_ = nullptr;

    CameraState currentState_;

    /// @brief ブレンドスタック（先頭が現在進行中のブレンド）
    std::stack<BlendLayer> blendStack_;

    float defaultBlendTime_ = 0.5f;
};

} // namespace GameEngine
