#pragma once
#include "CameraState.h"
#include "VirtualCamera.h"
#include <vector>
#include <climits>

namespace GameEngine {

class Camera;

/// @brief Cinemachineのメイン制御クラス
/// 複数のVirtualCameraを管理し、最終的なカメラ状態を計算
class CinemachineBrain {
public:
    CinemachineBrain() = default;
    ~CinemachineBrain() = default;

    /// @brief 初期化
    /// @param outputCamera 出力先のカメラ
    void Initialize(Camera* outputCamera);

    /// @brief 更新処理
    /// @param deltaTime フレーム時間
    void Update(float deltaTime);

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

private:
    VirtualCamera* FindHighestPriorityCamera() const;
    void BlendToCamera(VirtualCamera* newCamera);
    void UpdateBlend(float deltaTime);
    void ApplyStateToOutputCamera();

    std::vector<VirtualCamera*> virtualCameras_;
    Camera* outputCamera_ = nullptr;

    VirtualCamera* activeCamera_ = nullptr;
    VirtualCamera* previousCamera_ = nullptr;

    CameraState currentState_;
    CameraState blendStartState_;

    float blendProgress_ = 1.0f;
    float blendDuration_ = 0.0f;
    float defaultBlendTime_ = 0.5f;

    bool isBlending_ = false;
};

} // namespace GameEngine
