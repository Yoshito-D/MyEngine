#pragma once
#include "CameraState.h"
#include "ICinemachineComponent.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <algorithm>

namespace GameEngine {

/// @brief 仮想カメラの基底クラス
class VirtualCamera {
public:
    using ComponentFactory = std::function<ICinemachineComponent*(VirtualCamera&)>;

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

    /// @brief コンポーネント名からファクトリを登録
    static bool RegisterComponentFactory(const std::string& componentName, ComponentFactory factory);

    /// @brief コンポーネント名から追加する（登録済みファクトリ経由）
    ICinemachineComponent* AddComponentByName(const std::string& componentName);

    /// @brief コンポーネント名から取得する
    ICinemachineComponent* FindComponentByName(const std::string& componentName) const;

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

    // 名前
    const std::string& GetName() const { return name_; }
    void SetName(const std::string& name) { name_ = name; }

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

    /// @brief コンポーネント一覧を取得（読み取り専用）
    const std::vector<std::unique_ptr<ICinemachineComponent>>& GetComponents() const { return components_; }

    /// @brief 仮想カメラ設定を保存する
    nlohmann::json Serialize() const;

    /// @brief 仮想カメラ設定を読み込む
    void Deserialize(const nlohmann::json& data);

protected:
    void SortComponents();

    std::vector<std::unique_ptr<ICinemachineComponent>> components_;
    Transform* followTarget_ = nullptr;
    Transform* lookAtTarget_ = nullptr;
    CameraState state_;
    std::string name_;
    int priority_ = 0;
    bool isActive_ = true;
};

} // namespace GameEngine
