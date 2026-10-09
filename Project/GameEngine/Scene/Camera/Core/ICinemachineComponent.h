#pragma once
#include "GameEngine/Scene/Camera/Core/CameraState.h"
#include <nlohmann/json.hpp>

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
   /// @brief 未接続・未確保の状態を構築する。
   ICinemachineComponent() = default;
   /// @brief 所有者への接続やGPU実行状態を別の実体へ複製することを禁止する。
   ICinemachineComponent(const ICinemachineComponent&) = delete;
   /// @brief 所有境界を迂回するコピー代入を禁止する。
   ICinemachineComponent& operator=(const ICinemachineComponent&) = delete;

    /// @brief 派生コンポーネントを基底ポインター経由で安全に破棄する
    virtual ~ICinemachineComponent() = default;

    /// @brief 所有カメラへの編集用アクセス。
    VirtualCamera* GetOwnerCamera() { return owner_; }
    /// @brief 所有カメラの読み取り用アクセス。
    const VirtualCamera* GetOwnerCamera() const { return owner_; }

    /// @brief カメラ状態を変更する
    /// @param state 変更するカメラ状態
    /// @param deltaTime フレーム時間
    virtual void MutateCameraState(CameraState& state, float deltaTime) = 0;

    /// @brief 処理ステージを取得
    virtual CinemachineStage GetStage() const = 0;

    /// @brief 同一ステージ内の更新順を返す。小さい値を先に実行し、同値は追加順を保つ。
    virtual int GetExecutionOrder() const { return 0; }

    /// @brief コンポーネントが有効かどうか
    bool IsEnabled() const { return isEnabled_; }
    /// @brief カメラ状態へ反映するかを設定する
    void SetEnabled(bool enabled) { isEnabled_ = enabled; }

    /// @brief コンポーネント名を取得
    virtual const char* GetComponentName() const = 0;

    /// @brief コンポーネント固有パラメータを保存する
    virtual nlohmann::json Serialize() const { return nlohmann::json::object(); }

    /// @brief コンポーネント固有パラメータを読み込む
    virtual void Deserialize(const nlohmann::json& data) { (void)data; }

#ifdef USE_IMGUI
    /// @brief ImGuiによるインスペクタ表示（各コンポーネントが自身を描画）
    virtual void DrawInspector() = 0;
#endif

protected:
    /// @brief 所有登録後に呼び出される初期化フック。所有者の差し替えは行わない。
    virtual void OnAttach() {}
private:
    friend class VirtualCamera;
    void Attach(VirtualCamera& owner) { owner_ = &owner; OnAttach(); }
    VirtualCamera* owner_ = nullptr;
    bool isEnabled_ = true;
};

} // namespace GameEngine
