#include "pch.h"
#include "ParticleMeshModule.h"

namespace GameEngine {
nlohmann::json ParticleMeshModule::ToJson() const {
   return {
	  { "enabled", enabled_ },
	  { "particleMeshType", static_cast<int>(meshType_) },
	  { "meshOriginY", originY_ },
	  { "ringInnerRadius", ringInnerRadius_ },
	  { "ringOuterRadius", ringOuterRadius_ },
	  { "ringSegments", ringSegments_ },
	  { "sphereRadius", sphereRadius_ },
	  { "sphereStacks", sphereStacks_ },
	  { "sphereSlices", sphereSlices_ },
	  { "boxSize", { boxSize_.x, boxSize_.y, boxSize_.z } },
	  { "cylinderTopRadius", cylinderTopRadius_ },
	  { "cylinderBottomRadius", cylinderBottomRadius_ },
	  { "cylinderRadius", GetCylinderRadius() },
	  { "cylinderHeight", cylinderHeight_ },
	  { "cylinderSegments", cylinderSegments_ },
	  { "coneRadius", coneRadius_ },
	  { "coneHeight", coneHeight_ },
	  { "coneSegments", coneSegments_ },
	  { "circleRadius", circleRadius_ },
	  { "circleSegments", circleSegments_ },
	  { "planeWidth", planeWidth_ },
	  { "planeDepth", planeDepth_ },
	  { "torusMajorRadius", torusMajorRadius_ },
	  { "torusMinorRadius", torusMinorRadius_ },
	  { "torusMajorSegments", torusMajorSegments_ },
	  { "torusMinorSegments", torusMinorSegments_ }
   };
}

void ParticleMeshModule::FromJson(const nlohmann::json& j) {
   if (j.contains("enabled")) enabled_ = j["enabled"];
   if (j.contains("particleMeshType")) SetMeshType(static_cast<MeshType>(j["particleMeshType"].get<int>()));
   if (j.contains("meshOriginY")) SetOriginY(j["meshOriginY"].get<float>());
   if (j.contains("ringInnerRadius")) SetRingInnerRadius(j["ringInnerRadius"]);
   if (j.contains("ringOuterRadius")) SetRingOuterRadius(j["ringOuterRadius"]);
   if (j.contains("ringSegments")) SetRingSegments(j["ringSegments"].get<uint32_t>());
   if (j.contains("sphereRadius")) SetSphereRadius(j["sphereRadius"]);
   if (j.contains("sphereStacks")) SetSphereStacks(j["sphereStacks"].get<uint32_t>());
   if (j.contains("sphereSlices")) SetSphereSlices(j["sphereSlices"].get<uint32_t>());
   if (j.contains("boxSize") && j["boxSize"].is_array() && j["boxSize"].size() >= 3) {
	  SetBoxSize(Vector3(j["boxSize"][0], j["boxSize"][1], j["boxSize"][2]));
   }
   if (j.contains("cylinderRadius")) SetCylinderRadius(j["cylinderRadius"]);
   if (j.contains("cylinderTopRadius")) SetCylinderTopRadius(j["cylinderTopRadius"]);
   if (j.contains("cylinderBottomRadius")) SetCylinderBottomRadius(j["cylinderBottomRadius"]);
   if (j.contains("cylinderHeight")) SetCylinderHeight(j["cylinderHeight"]);
   if (j.contains("cylinderSegments")) SetCylinderSegments(j["cylinderSegments"].get<uint32_t>());
   if (j.contains("coneRadius")) SetConeRadius(j["coneRadius"]);
   if (j.contains("coneHeight")) SetConeHeight(j["coneHeight"]);
   if (j.contains("coneSegments")) SetConeSegments(j["coneSegments"].get<uint32_t>());
   if (j.contains("circleRadius")) SetCircleRadius(j["circleRadius"]);
   if (j.contains("circleSegments")) SetCircleSegments(j["circleSegments"].get<uint32_t>());
   if (j.contains("planeWidth")) SetPlaneWidth(j["planeWidth"]);
   if (j.contains("planeDepth")) SetPlaneDepth(j["planeDepth"]);
   if (j.contains("torusMajorRadius")) SetTorusMajorRadius(j["torusMajorRadius"]);
   if (j.contains("torusMinorRadius")) SetTorusMinorRadius(j["torusMinorRadius"]);
   if (j.contains("torusMajorSegments")) SetTorusMajorSegments(j["torusMajorSegments"].get<uint32_t>());
   if (j.contains("torusMinorSegments")) SetTorusMinorSegments(j["torusMinorSegments"].get<uint32_t>());
}
}
