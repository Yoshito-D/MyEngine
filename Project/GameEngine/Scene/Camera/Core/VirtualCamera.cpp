#include "pch.h"
#include "VirtualCamera.h"
#include <unordered_map>

namespace GameEngine {

namespace {
std::unordered_map<std::string, VirtualCamera::ComponentFactory>& ComponentFactories() {
    static std::unordered_map<std::string, VirtualCamera::ComponentFactory> factories;
    return factories;
}

nlohmann::json SerializeVector3(const Vector3& value) {
    return nlohmann::json::array({ value.x, value.y, value.z });
}

Vector3 DeserializeVector3(const nlohmann::json& data, const Vector3& fallback) {
    if (!data.is_array() || data.size() != 3) {
        return fallback;
    }
    return Vector3(
        data[0].get<float>(),
        data[1].get<float>(),
        data[2].get<float>());
}

nlohmann::json SerializeQuaternion(const Quaternion& value) {
    return nlohmann::json::array({ value.x, value.y, value.z, value.w });
}

Quaternion DeserializeQuaternion(const nlohmann::json& data, const Quaternion& fallback) {
    if (!data.is_array() || data.size() != 4) {
        return fallback;
    }
    return Quaternion{
        data[0].get<float>(),
        data[1].get<float>(),
        data[2].get<float>(),
        data[3].get<float>() };
}

nlohmann::json SerializeMatrix4x4(const Matrix4x4& matrix) {
    nlohmann::json rows = nlohmann::json::array();
    for (int row = 0; row < 4; ++row) {
        nlohmann::json columns = nlohmann::json::array();
        for (int column = 0; column < 4; ++column) {
            columns.push_back(matrix.m[row][column]);
        }
        rows.push_back(std::move(columns));
    }
    return rows;
}

Matrix4x4 DeserializeMatrix4x4(const nlohmann::json& data, const Matrix4x4& fallback) {
    if (!data.is_array() || data.size() != 4) {
        return fallback;
    }

    Matrix4x4 matrix = fallback;
    for (int row = 0; row < 4; ++row) {
        if (!data[row].is_array() || data[row].size() != 4) {
            return fallback;
        }
        for (int column = 0; column < 4; ++column) {
            matrix.m[row][column] = data[row][column].get<float>();
        }
    }
    return matrix;
}

nlohmann::json SerializeTransform(const Transform& transform) {
    const Quaternion activeQuaternion = transform.GetActiveQuaternion();
    const Vector3 activeEuler = transform.GetActiveEuler();
    return nlohmann::json{
        { "translation", SerializeVector3(transform.translation) },
        { "rotation", SerializeVector3(activeEuler) },
        { "rotationQuaternion", SerializeQuaternion(activeQuaternion) },
        { "rotationSource", transform.IsUsingQuaternion() ? "quaternion" : "euler" },
        { "scale", SerializeVector3(transform.scale) }
    };
}

Transform DeserializeTransform(const nlohmann::json& data, const Transform& fallback) {
    if (!data.is_object()) {
        return fallback;
    }

    Transform transform = fallback;
    transform.translation = data.contains("translation")
        ? DeserializeVector3(data.at("translation"), transform.translation)
        : transform.translation;
    transform.scale = data.contains("scale")
        ? DeserializeVector3(data.at("scale"), transform.scale)
        : transform.scale;

    const std::string rotationSource = data.value("rotationSource", "euler");
    if (rotationSource == "quaternion" && data.contains("rotationQuaternion")) {
        transform.SetRotationQuaternion(DeserializeQuaternion(data.at("rotationQuaternion"), transform.GetActiveQuaternion()));
    } else if (data.contains("rotation")) {
        transform.SetRotationEuler(DeserializeVector3(data.at("rotation"), transform.GetActiveEuler()));
    }

    return transform;
}

nlohmann::json SerializeCameraState(const CameraState& state) {
    return nlohmann::json{
        { "transform", SerializeTransform(state.transform) },
        { "fov", state.fov },
        { "nearClip", state.nearClip },
        { "farClip", state.farClip },
        { "hasViewMatrixOverride", state.hasViewMatrixOverride },
        { "viewMatrixOverride", SerializeMatrix4x4(state.viewMatrixOverride) }
    };
}

CameraState DeserializeCameraState(const nlohmann::json& data, const CameraState& fallback) {
    if (!data.is_object()) {
        return fallback;
    }

    CameraState state = fallback;
    if (data.contains("transform")) {
        state.transform = DeserializeTransform(data.at("transform"), state.transform);
    }
    if (data.contains("fov") && data.at("fov").is_number()) {
        state.fov = data.at("fov").get<float>();
    }
    if (data.contains("nearClip") && data.at("nearClip").is_number()) {
        state.nearClip = data.at("nearClip").get<float>();
    }
    if (data.contains("farClip") && data.at("farClip").is_number()) {
        state.farClip = data.at("farClip").get<float>();
    }
    if (data.contains("hasViewMatrixOverride") && data.at("hasViewMatrixOverride").is_boolean()) {
        state.hasViewMatrixOverride = data.at("hasViewMatrixOverride").get<bool>();
    }
    if (data.contains("viewMatrixOverride")) {
        state.viewMatrixOverride = DeserializeMatrix4x4(data.at("viewMatrixOverride"), state.viewMatrixOverride);
    }
    return state;
}
} // namespace

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

bool VirtualCamera::RegisterComponentFactory(const std::string& componentName, ComponentFactory factory) {
    if (componentName.empty() || !factory) {
        return false;
    }

    ComponentFactories()[componentName] = std::move(factory);
    return true;
}

ICinemachineComponent* VirtualCamera::AddComponentByName(const std::string& componentName) {
    if (componentName.empty()) {
        return nullptr;
    }

    if (auto* existing = FindComponentByName(componentName)) {
        return existing;
    }

    const auto& factories = ComponentFactories();
    const auto it = factories.find(componentName);
    if (it == factories.end()) {
        return nullptr;
    }

    return it->second(*this);
}

ICinemachineComponent* VirtualCamera::FindComponentByName(const std::string& componentName) const {
    if (componentName.empty()) {
        return nullptr;
    }

    for (const auto& component : components_) {
        if (component && componentName == component->GetComponentName()) {
            return component.get();
        }
    }
    return nullptr;
}

void VirtualCamera::SortComponents() {
    std::sort(components_.begin(), components_.end(),
        [](const std::unique_ptr<ICinemachineComponent>& a,
           const std::unique_ptr<ICinemachineComponent>& b) {
            return static_cast<int>(a->GetStage()) < static_cast<int>(b->GetStage());
        });
}

nlohmann::json VirtualCamera::Serialize() const {
    nlohmann::json componentsData = nlohmann::json::array();
    for (const auto& component : components_) {
        if (!component) {
            continue;
        }

        componentsData.push_back(nlohmann::json{
            { "componentName", component->GetComponentName() },
            { "enabled", component->IsEnabled() },
            { "data", component->Serialize() }
        });
    }

    return nlohmann::json{
        { "name", name_ },
        { "priority", priority_ },
        { "active", isActive_ },
        { "state", SerializeCameraState(state_) },
        { "components", componentsData }
    };
}

void VirtualCamera::Deserialize(const nlohmann::json& data) {
    if (!data.is_object()) {
        return;
    }

    if (data.contains("name") && data.at("name").is_string()) {
        name_ = data.at("name").get<std::string>();
    }
    if (data.contains("priority") && data.at("priority").is_number_integer()) {
        priority_ = data.at("priority").get<int>();
    }
    if (data.contains("active") && data.at("active").is_boolean()) {
        isActive_ = data.at("active").get<bool>();
    }
    if (data.contains("state") && data.at("state").is_object()) {
        state_ = DeserializeCameraState(data.at("state"), state_);
    }

    if (!data.contains("components") || !data.at("components").is_array()) {
        return;
    }

    for (const auto& componentData : data.at("components")) {
        if (!componentData.is_object()) {
            continue;
        }

        const std::string componentName = componentData.value("componentName", "");
        if (componentName.empty()) {
            continue;
        }

        ICinemachineComponent* component = FindComponentByName(componentName);
        if (!component) {
            component = AddComponentByName(componentName);
        }
        if (!component) {
            continue;
        }

        if (componentData.contains("enabled") && componentData.at("enabled").is_boolean()) {
            component->SetEnabled(componentData.at("enabled").get<bool>());
        }
        if (componentData.contains("data") && componentData.at("data").is_object()) {
            component->Deserialize(componentData.at("data"));
        }
    }
}

} // namespace GameEngine
