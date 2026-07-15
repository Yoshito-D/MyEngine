#include "pch.h"
#include "RendererModule.h"

namespace GameEngine {
    RendererModule::RendererModule() = default;

    nlohmann::json RendererModule::ToJson() const {
        nlohmann::json j;
        j["enabled"] = enabled_;
        j["rotationSpace"] = static_cast<int>(rotationSpace_);
        j["billboardType"] = static_cast<int>(billboardType_);
        j["speedScale"] = speedScale_;
        j["lengthScale"] = lengthScale_;
        j["velocityStretchEnabled"] = velocityStretchEnabled_;
        j["sortMode"] = static_cast<int>(sortMode_);
        j["cameraFadeEnabled"] = cameraFadeEnabled_;
        j["cameraFadeNear"] = cameraFadeNear_;
        j["cameraFadeFar"] = cameraFadeFar_;
        return j;
    }

    void RendererModule::FromJson(const nlohmann::json& j) {
        if (j.contains("enabled")) enabled_ = j["enabled"];
        if (j.contains("rotationSpace")) rotationSpace_ = static_cast<RotationSpace>(j["rotationSpace"].get<int>());
        if (j.contains("billboardType")) billboardType_ = static_cast<BillboardType>(j["billboardType"].get<int>());
        if (j.contains("speedScale")) speedScale_ = j["speedScale"];
        if (j.contains("lengthScale")) lengthScale_ = j["lengthScale"];
        if (j.contains("velocityStretchEnabled")) {
            velocityStretchEnabled_ = j["velocityStretchEnabled"];
        } else {
            // 旧データのVelocityは方向合わせとストレッチが一体だったため、見た目を維持する。
            velocityStretchEnabled_ = billboardType_ == BillboardType::Velocity;
        }
        if (j.contains("sortMode")) sortMode_ = static_cast<SortMode>(j["sortMode"].get<int>());
        if (j.contains("cameraFadeEnabled")) cameraFadeEnabled_ = j["cameraFadeEnabled"];
        if (j.contains("cameraFadeNear")) SetCameraFadeNear(j["cameraFadeNear"]);
        if (j.contains("cameraFadeFar")) SetCameraFadeFar(j["cameraFadeFar"]);
    }
}
