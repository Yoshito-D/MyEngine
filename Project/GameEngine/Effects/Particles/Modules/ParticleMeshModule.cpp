#include "GameEngine/pch.h"
#include <cstdint>
#include "GameEngine/Graphics/Resources/Mesh.h"
#include "GameEngine/Effects/Particles/Modules/ParticleMeshModule.h"

namespace GameEngine {
bool ParticleMeshModule::RebuildMesh(Mesh& mesh) {
   using MeshType = ParticleMeshModule::MeshType;

   // Mesh Moduleを無効化した場合も描画経路を変えず、既定Quadへ戻して既存エフェクトの見た目を維持する。
   const float meshOriginY = IsEnabled() ? GetOriginY() : 0.5f;
   const MeshType meshType = IsEnabled() ? GetMeshType() : MeshType::Quad;

   switch (meshType) {
      case MeshType::Quad:
         mesh.CreateParticleQuad(1.0f, 1.0f, Mesh::PlaneOrientation::XY, meshOriginY);
         break;
      case MeshType::Ring:
         mesh.CreateRing(GetRingInnerRadius(), GetRingOuterRadius(), GetRingSegments());
         break;
      case MeshType::Sphere:
         mesh.CreateSphere(GetSphereRadius(), GetSphereStacks(), GetSphereSlices(), meshOriginY);
         break;
      case MeshType::Box: {
         auto s = GetBoxSize();
         mesh.CreateBox(s.x, s.y, s.z, meshOriginY);
         break;
      }
      case MeshType::Cylinder:
         mesh.CreateCylinderWithoutCaps(
            GetCylinderTopRadius(), GetCylinderBottomRadius(), GetCylinderHeight(), GetCylinderSegments(), meshOriginY);
         break;
      case MeshType::Cone:
         mesh.CreateCone(GetConeRadius(), GetConeHeight(), GetConeSegments(), meshOriginY);
         break;
      case MeshType::Circle:
         mesh.CreateCircle(GetCircleRadius(), GetCircleSegments());
         break;
      case MeshType::Plane:
         mesh.CreatePlane(GetPlaneWidth(), GetPlaneDepth());
         break;
      case MeshType::Torus:
         mesh.CreateTorus(GetTorusMajorRadius(), GetTorusMinorRadius(), GetTorusMajorSegments(),
            GetTorusMinorSegments(), meshOriginY);
         break;
      case MeshType::Triangle:
         mesh.CreateTriangle();
         break;
      default:
         mesh.CreateParticleQuad(1.0f, 1.0f, Mesh::PlaneOrientation::XY, meshOriginY);
         break;
   }

   if (!mesh.GetVertexBufferView().BufferLocation || !mesh.GetIndexCount()) {
      return false;
   }

   meshDirty_ = false;
   return true;
}

nlohmann::json ParticleMeshModule::ToJson() const {
   return {{"enabled", IsEnabled()}, {"particleMeshType", static_cast<std::int32_t>(meshType_)},
      {"meshOriginY", originY_}, {"ringInnerRadius", ringInnerRadius_}, {"ringOuterRadius", ringOuterRadius_},
      {"ringSegments", ringSegments_}, {"sphereRadius", sphereRadius_}, {"sphereStacks", sphereStacks_},
      {"sphereSlices", sphereSlices_}, {"boxSize", {boxSize_.x, boxSize_.y, boxSize_.z}},
      {"cylinderTopRadius", cylinderTopRadius_}, {"cylinderBottomRadius", cylinderBottomRadius_},
      {"cylinderHeight", cylinderHeight_}, {"cylinderSegments", cylinderSegments_}, {"coneRadius", coneRadius_},
      {"coneHeight", coneHeight_}, {"coneSegments", coneSegments_}, {"circleRadius", circleRadius_},
      {"circleSegments", circleSegments_}, {"planeWidth", planeWidth_}, {"planeDepth", planeDepth_},
      {"torusMajorRadius", torusMajorRadius_}, {"torusMinorRadius", torusMinorRadius_},
      {"torusMajorSegments", torusMajorSegments_}, {"torusMinorSegments", torusMinorSegments_}};
}

void ParticleMeshModule::FromJson(const nlohmann::json& j) {
   if (j.contains("enabled")) {
      SetEnabled(j["enabled"].get<bool>());
   }

   if (j.contains("particleMeshType")) {
      SetMeshType(static_cast<MeshType>(j["particleMeshType"].get<std::int32_t>()));
   }

   if (j.contains("meshOriginY")) {
      SetOriginY(j["meshOriginY"].get<float>());
   }

   if (j.contains("ringInnerRadius")) {
      SetRingInnerRadius(j["ringInnerRadius"]);
   }

   if (j.contains("ringOuterRadius")) {
      SetRingOuterRadius(j["ringOuterRadius"]);
   }

   if (j.contains("ringSegments")) {
      SetRingSegments(j["ringSegments"].get<uint32_t>());
   }

   if (j.contains("sphereRadius")) {
      SetSphereRadius(j["sphereRadius"]);
   }

   if (j.contains("sphereStacks")) {
      SetSphereStacks(j["sphereStacks"].get<uint32_t>());
   }

   if (j.contains("sphereSlices")) {
      SetSphereSlices(j["sphereSlices"].get<uint32_t>());
   }

   if (j.contains("boxSize") && j["boxSize"].is_array() && j["boxSize"].size() >= 3) {
      SetBoxSize(Vector3(j["boxSize"][0], j["boxSize"][1], j["boxSize"][2]));
   }

   if (j.contains("cylinderTopRadius")) {
      SetCylinderTopRadius(j["cylinderTopRadius"]);
   }

   if (j.contains("cylinderBottomRadius")) {
      SetCylinderBottomRadius(j["cylinderBottomRadius"]);
   }

   if (j.contains("cylinderHeight")) {
      SetCylinderHeight(j["cylinderHeight"]);
   }

   if (j.contains("cylinderSegments")) {
      SetCylinderSegments(j["cylinderSegments"].get<uint32_t>());
   }

   if (j.contains("coneRadius")) {
      SetConeRadius(j["coneRadius"]);
   }

   if (j.contains("coneHeight")) {
      SetConeHeight(j["coneHeight"]);
   }

   if (j.contains("coneSegments")) {
      SetConeSegments(j["coneSegments"].get<uint32_t>());
   }

   if (j.contains("circleRadius")) {
      SetCircleRadius(j["circleRadius"]);
   }

   if (j.contains("circleSegments")) {
      SetCircleSegments(j["circleSegments"].get<uint32_t>());
   }

   if (j.contains("planeWidth")) {
      SetPlaneWidth(j["planeWidth"]);
   }

   if (j.contains("planeDepth")) {
      SetPlaneDepth(j["planeDepth"]);
   }

   if (j.contains("torusMajorRadius")) {
      SetTorusMajorRadius(j["torusMajorRadius"]);
   }

   if (j.contains("torusMinorRadius")) {
      SetTorusMinorRadius(j["torusMinorRadius"]);
   }

   if (j.contains("torusMajorSegments")) {
      SetTorusMajorSegments(j["torusMajorSegments"].get<uint32_t>());
   }

   if (j.contains("torusMinorSegments")) {
      SetTorusMinorSegments(j["torusMinorSegments"].get<uint32_t>());
   }
}
}

namespace GameEngine {

void ParticleMeshModule::SetMeshType(MeshType type) {
   meshType_ = type;
   meshDirty_ = true;
}

void ParticleMeshModule::SetOriginY(float originY) {
   originY_ = std::clamp(originY, 0.0f, 1.0f);
   meshDirty_ = true;
}

void ParticleMeshModule::SetRingInnerRadius(float radius) {
   ringInnerRadius_ = radius;
   meshDirty_ = true;
}

void ParticleMeshModule::SetRingOuterRadius(float radius) {
   ringOuterRadius_ = radius;
   meshDirty_ = true;
}

void ParticleMeshModule::SetRingSegments(uint32_t segments) {
   ringSegments_ = segments;
   meshDirty_ = true;
}

void ParticleMeshModule::SetSphereRadius(float radius) {
   sphereRadius_ = radius;
   meshDirty_ = true;
}

void ParticleMeshModule::SetSphereStacks(uint32_t stacks) {
   sphereStacks_ = stacks;
   meshDirty_ = true;
}

void ParticleMeshModule::SetSphereSlices(uint32_t slices) {
   sphereSlices_ = slices;
   meshDirty_ = true;
}

void ParticleMeshModule::SetBoxSize(const Vector3& size) {
   boxSize_ = size;
   meshDirty_ = true;
}

void ParticleMeshModule::SetCylinderTopRadius(float radius) {
   cylinderTopRadius_ = std::max(0.0f, radius);
   meshDirty_ = true;
}

void ParticleMeshModule::SetCylinderBottomRadius(float radius) {
   cylinderBottomRadius_ = std::max(0.0f, radius);
   meshDirty_ = true;
}

void ParticleMeshModule::SetCylinderRadius(float radius) {
   SetCylinderTopRadius(radius);
   SetCylinderBottomRadius(radius);
}

void ParticleMeshModule::SetCylinderHeight(float height) {
   cylinderHeight_ = height;
   meshDirty_ = true;
}

void ParticleMeshModule::SetCylinderSegments(uint32_t segments) {
   cylinderSegments_ = segments;
   meshDirty_ = true;
}

void ParticleMeshModule::SetConeRadius(float radius) {
   coneRadius_ = radius;
   meshDirty_ = true;
}

void ParticleMeshModule::SetConeHeight(float height) {
   coneHeight_ = height;
   meshDirty_ = true;
}

void ParticleMeshModule::SetConeSegments(uint32_t segments) {
   coneSegments_ = segments;
   meshDirty_ = true;
}

void ParticleMeshModule::SetCircleRadius(float radius) {
   circleRadius_ = radius;
   meshDirty_ = true;
}

void ParticleMeshModule::SetCircleSegments(uint32_t segments) {
   circleSegments_ = segments;
   meshDirty_ = true;
}

void ParticleMeshModule::SetPlaneWidth(float width) {
   planeWidth_ = width;
   meshDirty_ = true;
}

void ParticleMeshModule::SetPlaneDepth(float depth) {
   planeDepth_ = depth;
   meshDirty_ = true;
}

void ParticleMeshModule::SetTorusMajorRadius(float radius) {
   torusMajorRadius_ = radius;
   meshDirty_ = true;
}

void ParticleMeshModule::SetTorusMinorRadius(float radius) {
   torusMinorRadius_ = radius;
   meshDirty_ = true;
}

void ParticleMeshModule::SetTorusMajorSegments(uint32_t segments) {
   torusMajorSegments_ = segments;
   meshDirty_ = true;
}

void ParticleMeshModule::SetTorusMinorSegments(uint32_t segments) {
   torusMinorSegments_ = segments;
   meshDirty_ = true;
}

void ParticleMeshModule::SetEnabled(bool enabled) {
   if (IsEnabled() != enabled) {
      meshDirty_ = true;
   }

   ParticleModule::SetEnabled(enabled);
}

} // namespace GameEngine
