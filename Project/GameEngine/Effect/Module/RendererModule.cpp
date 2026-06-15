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

        j["particleMeshType"] = static_cast<int>(particleMeshType_);
        j["meshOriginY"] = meshOriginY_;
        // Ring
        j["ringInnerRadius"] = ringInnerRadius_;
        j["ringOuterRadius"] = ringOuterRadius_;
        j["ringSegments"] = ringSegments_;
        // Sphere
        j["sphereRadius"] = sphereRadius_;
        j["sphereStacks"] = sphereStacks_;
        j["sphereSlices"] = sphereSlices_;
        // Box
        j["boxSize"] = {boxSize_.x, boxSize_.y, boxSize_.z};
        // Cylinder
        j["cylinderTopRadius"] = cylinderTopRadius_;
        j["cylinderBottomRadius"] = cylinderBottomRadius_;
        j["cylinderRadius"] = GetCylinderRadius();
        j["cylinderHeight"] = cylinderHeight_;
        j["cylinderSegments"] = cylinderSegments_;
        // Cone
        j["coneRadius"] = coneRadius_;
        j["coneHeight"] = coneHeight_;
        j["coneSegments"] = coneSegments_;
        // Circle
        j["circleRadius"] = circleRadius_;
        j["circleSegments"] = circleSegments_;
        // Plane
        j["planeWidth"] = planeWidth_;
        j["planeDepth"] = planeDepth_;
        // Torus
        j["torusMajorRadius"] = torusMajorRadius_;
        j["torusMinorRadius"] = torusMinorRadius_;
        j["torusMajorSegments"] = torusMajorSegments_;
        j["torusMinorSegments"] = torusMinorSegments_;
        return j;
    }

    void RendererModule::FromJson(const nlohmann::json& j) {
        if (j.contains("enabled")) enabled_ = j["enabled"];
        if (j.contains("rotationSpace")) rotationSpace_ = static_cast<RotationSpace>(j["rotationSpace"].get<int>());
        if (j.contains("billboardType")) billboardType_ = static_cast<BillboardType>(j["billboardType"].get<int>());
        if (j.contains("speedScale")) speedScale_ = j["speedScale"];
        if (j.contains("lengthScale")) lengthScale_ = j["lengthScale"];

        if (j.contains("particleMeshType")) {
            particleMeshType_ = static_cast<ParticleMeshType>(j["particleMeshType"].get<int>());
            meshDirty_ = true;
        }
        if (j.contains("meshOriginY")) SetMeshOriginY(j["meshOriginY"].get<float>());
        if (j.contains("ringInnerRadius")) ringInnerRadius_ = j["ringInnerRadius"];
        if (j.contains("ringOuterRadius")) ringOuterRadius_ = j["ringOuterRadius"];
        if (j.contains("ringSegments")) ringSegments_ = j["ringSegments"].get<uint32_t>();
        if (j.contains("sphereRadius")) sphereRadius_ = j["sphereRadius"];
        if (j.contains("sphereStacks")) sphereStacks_ = j["sphereStacks"].get<uint32_t>();
        if (j.contains("sphereSlices")) sphereSlices_ = j["sphereSlices"].get<uint32_t>();
        if (j.contains("boxSize") && j["boxSize"].is_array()) {
            auto arr = j["boxSize"];
            boxSize_ = Vector3{arr[0], arr[1], arr[2]};
        }
        if (j.contains("cylinderRadius")) SetCylinderRadius(j["cylinderRadius"]);
        if (j.contains("cylinderTopRadius")) SetCylinderTopRadius(j["cylinderTopRadius"]);
        if (j.contains("cylinderBottomRadius")) SetCylinderBottomRadius(j["cylinderBottomRadius"]);
        if (j.contains("cylinderHeight")) cylinderHeight_ = j["cylinderHeight"];
        if (j.contains("cylinderSegments")) cylinderSegments_ = j["cylinderSegments"].get<uint32_t>();
        if (j.contains("coneRadius")) coneRadius_ = j["coneRadius"];
        if (j.contains("coneHeight")) coneHeight_ = j["coneHeight"];
        if (j.contains("coneSegments")) coneSegments_ = j["coneSegments"].get<uint32_t>();
        if (j.contains("circleRadius")) circleRadius_ = j["circleRadius"];
        if (j.contains("circleSegments")) circleSegments_ = j["circleSegments"].get<uint32_t>();
        if (j.contains("planeWidth")) planeWidth_ = j["planeWidth"];
        if (j.contains("planeDepth")) planeDepth_ = j["planeDepth"];
        if (j.contains("torusMajorRadius")) torusMajorRadius_ = j["torusMajorRadius"];
        if (j.contains("torusMinorRadius")) torusMinorRadius_ = j["torusMinorRadius"];
        if (j.contains("torusMajorSegments")) torusMajorSegments_ = j["torusMajorSegments"].get<uint32_t>();
        if (j.contains("torusMinorSegments")) torusMinorSegments_ = j["torusMinorSegments"].get<uint32_t>();
    }
}
