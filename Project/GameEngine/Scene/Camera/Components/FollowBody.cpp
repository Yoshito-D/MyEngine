#include "pch.h"
#include "FollowBody.h"
#include "../Core/VirtualCamera.h"
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace GameEngine {

void FollowBody::MutateCameraState(CameraState& state, float deltaTime) {
    if (owner_ == nullptr) return;

    Transform* target = owner_->GetFollowTarget();
    if (target == nullptr) return;

    // ターゲットのワールド位置 + オフセット
    Vector3 targetPosition = target->translation + offset_;

    // ダンピング付きの追従（指数減衰）
    float dampX = 1.0f - std::exp(-damping_.x * deltaTime);
    float dampY = 1.0f - std::exp(-damping_.y * deltaTime);
    float dampZ = 1.0f - std::exp(-damping_.z * deltaTime);

    state.transform.translation.x += (targetPosition.x - state.transform.translation.x) * dampX;
    state.transform.translation.y += (targetPosition.y - state.transform.translation.y) * dampY;
    state.transform.translation.z += (targetPosition.z - state.transform.translation.z) * dampZ;
}

#ifdef USE_IMGUI
void FollowBody::DrawInspector() {
    if (ImGui::Checkbox("Enabled", &isEnabled_)) {}

    float off[3] = { offset_.x, offset_.y, offset_.z };
    if (ImGui::DragFloat3("Offset", off, 0.1f)) {
        offset_ = { off[0], off[1], off[2] };
    }

    float damp[3] = { damping_.x, damping_.y, damping_.z };
    if (ImGui::DragFloat3("Damping", damp, 0.1f, 0.0f, 50.0f)) {
        damping_ = { damp[0], damp[1], damp[2] };
    }
}
#endif

} // namespace GameEngine
