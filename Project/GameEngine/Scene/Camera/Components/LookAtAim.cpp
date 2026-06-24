#include "pch.h"
#include "LookAtAim.h"
#include "../Core/VirtualCamera.h"
#include "Utility/MathUtils.h"
#include <cmath>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace GameEngine {

void LookAtAim::MutateCameraState(CameraState& state, float deltaTime) {
    if (owner_ == nullptr) return;

    Transform* target = owner_->GetLookAtTarget();
    if (target == nullptr) return;

    // ターゲット方向を計算
    Vector3 lookAtPosition = target->translation + offset_;
    Vector3 direction = lookAtPosition - state.transform.translation;

    if (direction.LengthSquared() < 1e-6f) return;

    direction = direction.Normalize();

    // 目標の回転を計算
    Quaternion targetRotation = LookRotation(direction, Vector3(0.0f, 1.0f, 0.0f));

    // ダンピング付きで回転を補間
    float t = 1.0f - std::exp(-damping_ * deltaTime);
    Quaternion newRotation = Quaternion::Slerp(state.transform.GetActiveQuaternion(), targetRotation, t);
    state.transform.SetRotationQuaternion(newRotation);
}

#ifdef USE_IMGUI
void LookAtAim::DrawInspector() {
    if (ImGui::Checkbox("Enabled", &isEnabled_)) {}

    if (ImGui::DragFloat("Damping", &damping_, 0.1f, 0.0f, 50.0f)) {}

    float off[3] = { offset_.x, offset_.y, offset_.z };
    if (ImGui::DragFloat3("Offset", off, 0.1f)) {
        offset_ = { off[0], off[1], off[2] };
    }
}
#endif

} // namespace GameEngine
