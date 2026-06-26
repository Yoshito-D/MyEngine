#include "pch.h"
#include "LookAtAim.h"
#include "../Core/VirtualCamera.h"
#include "Utility/MathUtils.h"
#include <cmath>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace GameEngine {

namespace {
const bool kRegistered = VirtualCamera::RegisterComponentFactory(
    "LookAtAim",
    [](VirtualCamera& camera) -> ICinemachineComponent* {
        if (auto* existing = camera.GetComponent<LookAtAim>()) {
            return existing;
        }
        return camera.AddComponent<LookAtAim>();
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

float ReadFloat(const nlohmann::json& data, const char* key, float fallback) {
    return data.contains(key) && data.at(key).is_number() ? data.at(key).get<float>() : fallback;
}
} // namespace

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

nlohmann::json LookAtAim::Serialize() const {
    return nlohmann::json{
        { "damping", damping_ },
        { "offset", SerializeVector3(offset_) }
    };
}

void LookAtAim::Deserialize(const nlohmann::json& data) {
    if (!data.is_object()) {
        return;
    }
    damping_ = ReadFloat(data, "damping", damping_);
    if (data.contains("offset")) {
        offset_ = DeserializeVector3(data.at("offset"), offset_);
    }
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
