#include "pch.h"
#include "RendererModule.h"

namespace GameEngine {
    RendererModule::RendererModule() = default;

    nlohmann::json RendererModule::ToJson() const {
        nlohmann::json j;
        j["enabled"] = enabled_;
        j["billboardType"] = static_cast<int>(billboardType_);
        j["speedScale"] = speedScale_;
        j["lengthScale"] = lengthScale_;
        return j;
    }

    void RendererModule::FromJson(const nlohmann::json& j) {
        if (j.contains("enabled")) enabled_ = j["enabled"];
        if (j.contains("billboardType")) billboardType_ = static_cast<BillboardType>(j["billboardType"].get<int>());
        if (j.contains("speedScale")) speedScale_ = j["speedScale"];
        if (j.contains("lengthScale")) lengthScale_ = j["lengthScale"];
    }
}
