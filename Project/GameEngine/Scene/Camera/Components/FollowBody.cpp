#include "pch.h"
#include "FollowBody.h"
#include "../Core/VirtualCamera.h"
#include <cmath>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace GameEngine {

namespace {
const bool kRegistered = VirtualCamera::RegisterComponentFactory(
    "FollowBody",
    [](VirtualCamera& camera) -> ICinemachineComponent* {
        if (auto* existing = camera.GetComponent<FollowBody>()) {
            return existing;
        }
        return camera.AddComponent<FollowBody>();
    });

nlohmann::json SerializeVector3(const Vector3& value) {
    return nlohmann::json::array({ value.x, value.y, value.z });
}

Vector3 DeserializeVector3(const nlohmann::json& data, const Vector3& fallback) {
    if (!data.is_array() || data.size() != 3) {
        return fallback;
    }
    return Vector3(data[0].get<float>(), data[1].get<float>(), data[2].get<float>());
}
} // namespace

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

nlohmann::json FollowBody::Serialize() const {
    return nlohmann::json{
        { "offset", SerializeVector3(offset_) },
        { "damping", SerializeVector3(damping_) }
    };
}

void FollowBody::Deserialize(const nlohmann::json& data) {
    if (!data.is_object()) {
        return;
    }
    if (data.contains("offset")) {
        offset_ = DeserializeVector3(data.at("offset"), offset_);
    }
    if (data.contains("damping")) {
        damping_ = DeserializeVector3(data.at("damping"), damping_);
    }
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
