#pragma once
#include "ParticleModule.h"
#include "Utility/VectorMath.h"
#include <algorithm>
#include <nlohmann/json.hpp>

namespace GameEngine {
class RendererModule : public ParticleModule {
public:
   enum class RotationSpace {
	  World = 0,
	  Local = 1
   };

   enum class BillboardType {
	  None = 0,
	  View,
	  Horizontal,
	  Vertical,
	  Velocity
   };

   enum class ParticleMeshType {
	  Quad = 0,
	  Ring,
	  Sphere,
	  Box,
	  Cylinder,
	  Cone,
	  Circle,
	  Plane,
	  Torus,
	  Triangle
   };

   RendererModule();

   void SetRotationSpace(RotationSpace space) { rotationSpace_ = space; }
   RotationSpace GetRotationSpace() const { return rotationSpace_; }

   void SetBillboardType(BillboardType type) { billboardType_ = type; }
   BillboardType GetBillboardType() const { return billboardType_; }

   void SetSpeedScale(float scale) { speedScale_ = scale; }
   float GetSpeedScale() const { return speedScale_; }

   void SetLengthScale(float scale) { lengthScale_ = scale; }
   float GetLengthScale() const { return lengthScale_; }

   void SetParticleMeshType(ParticleMeshType type) { particleMeshType_ = type; meshDirty_ = true; }
   ParticleMeshType GetParticleMeshType() const { return particleMeshType_; }

   void SetMeshOriginY(float originY) { meshOriginY_ = std::clamp(originY, 0.0f, 1.0f); meshDirty_ = true; }
   float GetMeshOriginY() const { return meshOriginY_; }

   void SetRingInnerRadius(float r) { ringInnerRadius_ = r; meshDirty_ = true; }
   float GetRingInnerRadius() const { return ringInnerRadius_; }
   void SetRingOuterRadius(float r) { ringOuterRadius_ = r; meshDirty_ = true; }
   float GetRingOuterRadius() const { return ringOuterRadius_; }
   void SetRingSegments(uint32_t s) { ringSegments_ = s; meshDirty_ = true; }
   uint32_t GetRingSegments() const { return ringSegments_; }

   void SetSphereRadius(float r) { sphereRadius_ = r; meshDirty_ = true; }
   float GetSphereRadius() const { return sphereRadius_; }
   void SetSphereStacks(uint32_t s) { sphereStacks_ = s; meshDirty_ = true; }
   uint32_t GetSphereStacks() const { return sphereStacks_; }
   void SetSphereSlices(uint32_t s) { sphereSlices_ = s; meshDirty_ = true; }
   uint32_t GetSphereSlices() const { return sphereSlices_; }

   void SetBoxSize(const Vector3& size) { boxSize_ = size; meshDirty_ = true; }
   const Vector3& GetBoxSize() const { return boxSize_; }

   void SetCylinderTopRadius(float r) { cylinderTopRadius_ = std::max(0.0f, r); meshDirty_ = true; }
   float GetCylinderTopRadius() const { return cylinderTopRadius_; }
   void SetCylinderBottomRadius(float r) { cylinderBottomRadius_ = std::max(0.0f, r); meshDirty_ = true; }
   float GetCylinderBottomRadius() const { return cylinderBottomRadius_; }
   void SetCylinderRadius(float r) { SetCylinderTopRadius(r); SetCylinderBottomRadius(r); }
   float GetCylinderRadius() const { return (cylinderTopRadius_ + cylinderBottomRadius_) * 0.5f; }
   void SetCylinderHeight(float h) { cylinderHeight_ = h; meshDirty_ = true; }
   float GetCylinderHeight() const { return cylinderHeight_; }
   void SetCylinderSegments(uint32_t s) { cylinderSegments_ = s; meshDirty_ = true; }
   uint32_t GetCylinderSegments() const { return cylinderSegments_; }

   void SetConeRadius(float r) { coneRadius_ = r; meshDirty_ = true; }
   float GetConeRadius() const { return coneRadius_; }
   void SetConeHeight(float h) { coneHeight_ = h; meshDirty_ = true; }
   float GetConeHeight() const { return coneHeight_; }
   void SetConeSegments(uint32_t s) { coneSegments_ = s; meshDirty_ = true; }
   uint32_t GetConeSegments() const { return coneSegments_; }

   void SetCircleRadius(float r) { circleRadius_ = r; meshDirty_ = true; }
   float GetCircleRadius() const { return circleRadius_; }
   void SetCircleSegments(uint32_t s) { circleSegments_ = s; meshDirty_ = true; }
   uint32_t GetCircleSegments() const { return circleSegments_; }

   void SetPlaneWidth(float w) { planeWidth_ = w; meshDirty_ = true; }
   float GetPlaneWidth() const { return planeWidth_; }
   void SetPlaneDepth(float d) { planeDepth_ = d; meshDirty_ = true; }
   float GetPlaneDepth() const { return planeDepth_; }

   void SetTorusMajorRadius(float r) { torusMajorRadius_ = r; meshDirty_ = true; }
   float GetTorusMajorRadius() const { return torusMajorRadius_; }
   void SetTorusMinorRadius(float r) { torusMinorRadius_ = r; meshDirty_ = true; }
   float GetTorusMinorRadius() const { return torusMinorRadius_; }
   void SetTorusMajorSegments(uint32_t s) { torusMajorSegments_ = s; meshDirty_ = true; }
   uint32_t GetTorusMajorSegments() const { return torusMajorSegments_; }
   void SetTorusMinorSegments(uint32_t s) { torusMinorSegments_ = s; meshDirty_ = true; }
   uint32_t GetTorusMinorSegments() const { return torusMinorSegments_; }

   bool IsMeshDirty() const { return meshDirty_; }
   void ClearMeshDirty() { meshDirty_ = false; }

   nlohmann::json ToJson() const override;
   void FromJson(const nlohmann::json& json) override;

private:
   RotationSpace rotationSpace_ = RotationSpace::Local;
   BillboardType billboardType_ = BillboardType::View;
   float speedScale_ = 1.0f;
   float lengthScale_ = 2.0f;
   ParticleMeshType particleMeshType_ = ParticleMeshType::Quad;
   float meshOriginY_ = 0.5f;
   bool meshDirty_ = false;
   float ringInnerRadius_ = 0.4f;
   float ringOuterRadius_ = 0.5f;
   uint32_t ringSegments_ = 32;
   float sphereRadius_ = 0.5f;
   uint32_t sphereStacks_ = 16;
   uint32_t sphereSlices_ = 32;
   Vector3 boxSize_{ 1.0f, 1.0f, 1.0f };
   float cylinderTopRadius_ = 0.5f;
   float cylinderBottomRadius_ = 0.5f;
   float cylinderHeight_ = 1.0f;
   uint32_t cylinderSegments_ = 32;
   float coneRadius_ = 0.5f;
   float coneHeight_ = 1.0f;
   uint32_t coneSegments_ = 32;
   float circleRadius_ = 0.5f;
   uint32_t circleSegments_ = 32;
   float planeWidth_ = 1.0f;
   float planeDepth_ = 1.0f;
   float torusMajorRadius_ = 0.5f;
   float torusMinorRadius_ = 0.2f;
   uint32_t torusMajorSegments_ = 32;
   uint32_t torusMinorSegments_ = 16;
};
}
