#pragma once
#include "ParticleModule.h"
#include "Utility/VectorMath.h"
#include "MainModule.h"
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

   /// @brief 透明パーティクルの描画順序
   enum class SortMode {
	  Auto = 0,
	  None,
	  BackToFront,
	  FrontToBack
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

   /// @brief 速度に応じて粒子本体を長軸方向へ引き延ばすか設定する
   /// @param enabled trueの場合、トレイルとは独立して速度ストレッチを適用する
   void SetVelocityStretchEnabled(bool enabled) { velocityStretchEnabled_ = enabled; }

   /// @brief 速度ストレッチが有効か取得する
   /// @return 有効な場合true
   bool IsVelocityStretchEnabled() const { return velocityStretchEnabled_; }

   /// @brief 描画順序を設定する
   void SetSortMode(SortMode mode) { sortMode_ = mode; }

   /// @brief 描画順序を取得する
   SortMode GetSortMode() const { return sortMode_; }

   /// @brief カメラ近接フェードを有効化する
   void SetCameraFadeEnabled(bool enabled) { cameraFadeEnabled_ = enabled; }

   /// @brief カメラ近接フェードが有効か取得する
   bool IsCameraFadeEnabled() const { return cameraFadeEnabled_; }

   /// @brief 完全に透明になるカメラ距離を設定する
   void SetCameraFadeNear(float distance) { cameraFadeNear_ = (std::max)(distance, 0.0f); }

   /// @brief 完全に透明になるカメラ距離を取得する
   float GetCameraFadeNear() const { return cameraFadeNear_; }

   /// @brief 完全に表示されるカメラ距離を設定する
   void SetCameraFadeFar(float distance) { cameraFadeFar_ = (std::max)(distance, 0.0f); }

   /// @brief 完全に表示されるカメラ距離を取得する
   float GetCameraFadeFar() const { return cameraFadeFar_; }

   /// @brief 粒子の位置履歴を帯として描画するリボンを有効化する
   void SetRibbonEnabled(bool enabled) { ribbonEnabled_ = enabled; }

   /// @brief リボン描画が有効か取得する
   bool IsRibbonEnabled() const { return ribbonEnabled_; }

   /// @brief 粒子ごとのリボン幅範囲を設定する
   void SetRibbonWidthRange(const RandomFloat& width) { ribbonWidth_ = width; }

   /// @brief 粒子ごとのリボン幅範囲を取得する
   const RandomFloat& GetRibbonWidthRange() const { return ribbonWidth_; }

   /// @brief 1粒子が保持する履歴点数を設定する
   void SetRibbonMaxPoints(uint32_t count) { ribbonMaxPoints_ = std::clamp(count, 2u, 128u); }

   /// @brief 1粒子が保持する履歴点数を取得する
   uint32_t GetRibbonMaxPoints() const { return ribbonMaxPoints_; }

   /// @brief 履歴点を追加する最小移動距離を設定する
   void SetRibbonMinDistance(float distance) { ribbonMinDistance_ = (std::max)(distance, 0.0001f); }

   /// @brief 履歴点を追加する最小移動距離を取得する
   float GetRibbonMinDistance() const { return ribbonMinDistance_; }

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

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

private:
   RotationSpace rotationSpace_ = RotationSpace::Local;
   BillboardType billboardType_ = BillboardType::View;
   float speedScale_ = 1.0f;
   float lengthScale_ = 2.0f;
   bool velocityStretchEnabled_ = false;
   SortMode sortMode_ = SortMode::Auto;
   bool cameraFadeEnabled_ = false;
   float cameraFadeNear_ = 0.25f;
   float cameraFadeFar_ = 1.0f;
   bool ribbonEnabled_ = false;
   RandomFloat ribbonWidth_{ 0.5f, 0.5f, false };
   uint32_t ribbonMaxPoints_ = 16;
   float ribbonMinDistance_ = 0.1f;
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
