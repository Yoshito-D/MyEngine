#include "pch.h"
#include "VirtualCamera.h"

namespace GameEngine {

void VirtualCamera::Initialize(const CameraState& initialState) {
    state_ = initialState;
}

void VirtualCamera::Update(float deltaTime) {
    state_ = CalculateState(deltaTime);
}

CameraState VirtualCamera::CalculateState(float deltaTime) {
    CameraState result = state_;

    // Body -> Aim -> Noise の順でコンポーネントを適用
    for (const auto& component : components_) {
        if (component && component->IsEnabled()) {
            component->MutateCameraState(result, deltaTime);
        }
    }

    return result;
}

void VirtualCamera::RemoveComponent(ICinemachineComponent* component) {
    components_.erase(
        std::remove_if(components_.begin(), components_.end(),
            [component](const std::unique_ptr<ICinemachineComponent>& c) {
                return c.get() == component;
            }),
        components_.end());
}

void VirtualCamera::SortComponents() {
    std::sort(components_.begin(), components_.end(),
        [](const std::unique_ptr<ICinemachineComponent>& a,
           const std::unique_ptr<ICinemachineComponent>& b) {
            return static_cast<int>(a->GetStage()) < static_cast<int>(b->GetStage());
        });
}

} // namespace GameEngine
