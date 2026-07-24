#pragma once
#include "ParticleModule.h"
#include "Utility/VectorMath.h"
#include <algorithm>

namespace GameEngine {
/// @brief パーティクル本体に使用するプロシージャルメッシュ設定を管理する
class ParticleMeshModule final : public ParticleModule {
public:
   /// @brief パーティクルメッシュ種別
   enum class MeshType {
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

   /// @brief パーティクルメッシュモジュールを既定値で構築する
   ParticleMeshModule() = default;

   /// @brief メッシュ種別を設定する
   /// @param type メッシュ種別
   void SetMeshType(MeshType type) { meshType_ = type; meshDirty_ = true; }

   /// @brief メッシュ種別を取得する
   /// @return メッシュ種別
   MeshType GetMeshType() const { return meshType_; }

   /// @brief メッシュのY原点を設定する
   /// @param originY 0が下端、0.5が中央、1が上端
   void SetOriginY(float originY) { originY_ = std::clamp(originY, 0.0f, 1.0f); meshDirty_ = true; }

   /// @brief メッシュのY原点を取得する
   /// @return 0から1のY原点
   float GetOriginY() const { return originY_; }

   /// @brief リング内半径を設定する
   void SetRingInnerRadius(float radius) { ringInnerRadius_ = radius; meshDirty_ = true; }
   /// @brief リング内半径を取得する
   float GetRingInnerRadius() const { return ringInnerRadius_; }
   /// @brief リング外半径を設定する
   void SetRingOuterRadius(float radius) { ringOuterRadius_ = radius; meshDirty_ = true; }
   /// @brief リング外半径を取得する
   float GetRingOuterRadius() const { return ringOuterRadius_; }
   /// @brief リング分割数を設定する
   void SetRingSegments(uint32_t segments) { ringSegments_ = segments; meshDirty_ = true; }
   /// @brief リング分割数を取得する
   uint32_t GetRingSegments() const { return ringSegments_; }

   /// @brief 球の半径を設定する
   void SetSphereRadius(float radius) { sphereRadius_ = radius; meshDirty_ = true; }
   /// @brief 球の半径を取得する
   float GetSphereRadius() const { return sphereRadius_; }
   /// @brief 球のスタック数を設定する
   void SetSphereStacks(uint32_t stacks) { sphereStacks_ = stacks; meshDirty_ = true; }
   /// @brief 球のスタック数を取得する
   uint32_t GetSphereStacks() const { return sphereStacks_; }
   /// @brief 球のスライス数を設定する
   void SetSphereSlices(uint32_t slices) { sphereSlices_ = slices; meshDirty_ = true; }
   /// @brief 球のスライス数を取得する
   uint32_t GetSphereSlices() const { return sphereSlices_; }

   /// @brief 箱の大きさを設定する
   void SetBoxSize(const Vector3& size) { boxSize_ = size; meshDirty_ = true; }
   /// @brief 箱の大きさを取得する
   const Vector3& GetBoxSize() const { return boxSize_; }

   /// @brief 円柱の上半径を設定する
   void SetCylinderTopRadius(float radius) { cylinderTopRadius_ = std::max(0.0f, radius); meshDirty_ = true; }
   /// @brief 円柱の上半径を取得する
   float GetCylinderTopRadius() const { return cylinderTopRadius_; }
   /// @brief 円柱の下半径を設定する
   void SetCylinderBottomRadius(float radius) { cylinderBottomRadius_ = std::max(0.0f, radius); meshDirty_ = true; }
   /// @brief 円柱の下半径を取得する
   float GetCylinderBottomRadius() const { return cylinderBottomRadius_; }
   /// @brief 円柱の上下半径を同時に設定する
   void SetCylinderRadius(float radius) { SetCylinderTopRadius(radius); SetCylinderBottomRadius(radius); }
   /// @brief 円柱の平均半径を取得する
   float GetCylinderRadius() const { return (cylinderTopRadius_ + cylinderBottomRadius_) * 0.5f; }
   /// @brief 円柱の高さを設定する
   void SetCylinderHeight(float height) { cylinderHeight_ = height; meshDirty_ = true; }
   /// @brief 円柱の高さを取得する
   float GetCylinderHeight() const { return cylinderHeight_; }
   /// @brief 円柱の分割数を設定する
   void SetCylinderSegments(uint32_t segments) { cylinderSegments_ = segments; meshDirty_ = true; }
   /// @brief 円柱の分割数を取得する
   uint32_t GetCylinderSegments() const { return cylinderSegments_; }

   /// @brief 円錐の半径を設定する
   void SetConeRadius(float radius) { coneRadius_ = radius; meshDirty_ = true; }
   /// @brief 円錐の半径を取得する
   float GetConeRadius() const { return coneRadius_; }
   /// @brief 円錐の高さを設定する
   void SetConeHeight(float height) { coneHeight_ = height; meshDirty_ = true; }
   /// @brief 円錐の高さを取得する
   float GetConeHeight() const { return coneHeight_; }
   /// @brief 円錐の分割数を設定する
   void SetConeSegments(uint32_t segments) { coneSegments_ = segments; meshDirty_ = true; }
   /// @brief 円錐の分割数を取得する
   uint32_t GetConeSegments() const { return coneSegments_; }

   /// @brief 円の半径を設定する
   void SetCircleRadius(float radius) { circleRadius_ = radius; meshDirty_ = true; }
   /// @brief 円の半径を取得する
   float GetCircleRadius() const { return circleRadius_; }
   /// @brief 円の分割数を設定する
   void SetCircleSegments(uint32_t segments) { circleSegments_ = segments; meshDirty_ = true; }
   /// @brief 円の分割数を取得する
   uint32_t GetCircleSegments() const { return circleSegments_; }

   /// @brief 平面の幅を設定する
   void SetPlaneWidth(float width) { planeWidth_ = width; meshDirty_ = true; }
   /// @brief 平面の幅を取得する
   float GetPlaneWidth() const { return planeWidth_; }
   /// @brief 平面の奥行きを設定する
   void SetPlaneDepth(float depth) { planeDepth_ = depth; meshDirty_ = true; }
   /// @brief 平面の奥行きを取得する
   float GetPlaneDepth() const { return planeDepth_; }

   /// @brief トーラスの主半径を設定する
   void SetTorusMajorRadius(float radius) { torusMajorRadius_ = radius; meshDirty_ = true; }
   /// @brief トーラスの主半径を取得する
   float GetTorusMajorRadius() const { return torusMajorRadius_; }
   /// @brief トーラスの副半径を設定する
   void SetTorusMinorRadius(float radius) { torusMinorRadius_ = radius; meshDirty_ = true; }
   /// @brief トーラスの副半径を取得する
   float GetTorusMinorRadius() const { return torusMinorRadius_; }
   /// @brief トーラスの主分割数を設定する
   void SetTorusMajorSegments(uint32_t segments) { torusMajorSegments_ = segments; meshDirty_ = true; }
   /// @brief トーラスの主分割数を取得する
   uint32_t GetTorusMajorSegments() const { return torusMajorSegments_; }
   /// @brief トーラスの副分割数を設定する
   void SetTorusMinorSegments(uint32_t segments) { torusMinorSegments_ = segments; meshDirty_ = true; }
   /// @brief トーラスの副分割数を取得する
   uint32_t GetTorusMinorSegments() const { return torusMinorSegments_; }

   /// @brief メッシュ再構築が必要か取得する
   bool IsDirty() const { return meshDirty_; }
   /// @brief メッシュ再構築済みとして扱う
   void ClearDirty() { meshDirty_ = false; }

   /// @brief メッシュ設定をJSONへ変換する
   nlohmann::json ToJson() const override;
   /// @brief メッシュ設定をJSONから読み込む
   void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI
   /// @brief メッシュ専用インスペクターを描画する
   void DrawInspector() override;
#endif

private:
   MeshType meshType_ = MeshType::Quad;
   float originY_ = 0.5f;
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
