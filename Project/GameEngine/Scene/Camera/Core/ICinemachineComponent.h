#pragma once
#include "CameraState.h"

namespace GameEngine {

class VirtualCamera;

/// @brief カメラコンポーネントの処理ステージ
enum class CinemachineStage {
    Body,   // 位置の計算
    Aim,    // 回転の計算
    Noise   // ノイズ/エフェクトの適用
};

/// @brief Cinemachineコンポーネントのインターフェース
class ICinemachineComponent {
public:
    virtual ~ICinemachineComponent() = default;

    /// @brief コンポーネントの初期化
    /// @param owner 所有するVirtualCamera
    virtual void Initialize(VirtualCamera* owner) { owner_ = owner; }

    /// @brief カメラ状態を変更する
    /// @param state 変更するカメラ状態
    /// @param deltaTime フレーム時間
    virtual void MutateCameraState(CameraState& state, float deltaTime) = 0;

    /// @brief 処理ステージを取得
    virtual CinemachineStage GetStage() const = 0;

    /// @brief コンポーネントが有効かどうか
    bool IsEnabled() const { return isEnabled_; }
    void SetEnabled(bool enabled) { isEnabled_ = enabled; }

    /// @brief コンポーネント名を取得
    virtual const char* GetComponentName() const = 0;

#ifdef USE_IMGUI
    /// @brief ImGuiによるインスペクタ表示（各コンポーネントが自身を描画）
    virtual void DrawInspector() = 0;
#endif

protected:
    VirtualCamera* owner_ = nullptr;
    bool isEnabled_ = true;
};

} // namespace GameEngine
