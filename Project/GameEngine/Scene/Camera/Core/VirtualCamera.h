#pragma once
#include "CameraState.h"
#include "ICinemachineComponent.h"
#include <vector>
#include <memory>
#include <algorithm>

namespace GameEngine {

/// @brief 仮想カメラの基底クラス
class VirtualCamera {
public:
    VirtualCamera() = default;
    virtual ~VirtualCamera() = default;

    /// @brief 初期化
    /// @param initialState 初期カメラ状態
    virtual void Initialize(const CameraState& initialState = CameraState());

    /// @brief 更新処理
    /// @param deltaTime フレーム時間
    virtual void Update(float deltaTime);

    /// @brief カメラ状態を計算
    /// @param deltaTime フレーム時間
    /// @return 計算されたカメラ状態
    CameraState CalculateState(float deltaTime);

    /// @brief コンポーネントを追加
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args) {
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = component.get();
        component->Initialize(this);
        components_.push_back(std::move(component));
        SortComponents();
        return ptr;
    }

    /// @brief コンポーネントを削除
    void RemoveComponent(ICinemachineComponent* component);

    /// @brief コンポーネントを取得
    template<typename T>
    T* GetComponent() const {
        for (const auto& comp : components_) {
            if (T* casted = dynamic_cast<T*>(comp.get())) {
                return casted;
            }
        }
        return nullptr;
    }

    // ターゲット設定
    void SetFollowTarget(Transform* target) { followTarget_ = target; }
    void SetLookAtTarget(Transform* target) { lookAtTarget_ = target; }
    Transform* GetFollowTarget() const { return followTarget_; }
    Transform* GetLookAtTarget() const { return lookAtTarget_; }

    // 優先度
    int GetPriority() const { return priority_; }
    void SetPriority(int priority) { priority_ = priority; }

    // 有効状態
    bool IsActive() const { return isActive_; }
    void SetActive(bool active) { isActive_ = active; }

    // カメラ状態アクセス
    const CameraState& GetState() const { return state_; }
    void SetState(const CameraState& state) { state_ = state; }

protected:
    void SortComponents();

    std::vector<std::unique_ptr<ICinemachineComponent>> components_;
    Transform* followTarget_ = nullptr;
    Transform* lookAtTarget_ = nullptr;
    CameraState state_;
    int priority_ = 0;
    bool isActive_ = true;
};

} // namespace GameEngine
