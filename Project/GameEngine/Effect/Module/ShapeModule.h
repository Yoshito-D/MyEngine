#pragma once
#include "Utility/VectorMath.h"
#include "Utility/Math/Transform.h"
#include <nlohmann/json.hpp>
#include "ParticleModule.h"
#include <algorithm>

namespace GameEngine {
class ModelAsset;
struct SkinCluster;
// ============================================================
// Shape Module (形状モジュール)
// パーティクルが発生する形状を決定
// ============================================================
class ShapeModule {
public:
   enum class ShapeType {
	  Sphere,      // 球
	  Hemisphere,  // 半球
	  Cone,        // 円錐
	  Box,         // 箱
	  Circle,      // 円
	  Edge,        // エッジ
	  Point,       // 点
	  Cylinder,    // 円柱（既存JSONの列挙値を維持するため末尾に追加）
	  Torus,       // トーラス
	  SkinnedMesh  // スキニング済みメッシュ表面
   };

   enum class EmitFrom {
	  Volume,      // 体積全体
	  Shell,       // 表面のみ
	  Edge         // エッジのみ
   };

   ShapeModule();

   void SetEnabled(bool enabled) { enabled_ = enabled; }
   bool IsEnabled() const { return enabled_; }

   void SetShapeType(ShapeType type) { shapeType_ = type; }
   ShapeType GetShapeType() const { return shapeType_; }

   void SetEmitFrom(EmitFrom from) { emitFrom_ = from; }
   EmitFrom GetEmitFrom() const { return emitFrom_; }

   // Sphere / Hemisphere
   void SetRadius(float radius) { radius_ = radius; }
   float GetRadius() const { return radius_; }

   // Cone
   void SetAngle(float angle) { angle_ = angle; }
   float GetAngle() const { return angle_; }
   void SetLength(float length) { length_ = length; }
   float GetLength() const { return length_; }

   // Box
   void SetBoxSize(const Vector3& size) { boxSize_ = size; }
   const Vector3& GetBoxSize() const { return boxSize_; }

   // Circle
   void SetArc(float arc) { arc_ = arc; }
   float GetArc() const { return arc_; }
   void SetCircleOutwardVelocity(float velocity) { circleOutwardVelocity_ = velocity; }
   float GetCircleOutwardVelocity() const { return circleOutwardVelocity_; }

   /// @brief トーラスの主半径を設定する
   void SetTorusMajorRadius(float radius) { torusMajorRadius_ = std::max(radius, 0.0f); }

   /// @brief トーラスの主半径を取得する
   float GetTorusMajorRadius() const { return torusMajorRadius_; }

   // Position
   void SetPosition(const Vector3& position) { transform_.translation = position; }
   const Vector3& GetPosition() const { return transform_.translation; }

   // Rotation
   void SetRotation(const Quaternion& rotation) { transform_.SetRotationQuaternion(rotation); }
   Quaternion GetRotationQuaternion() const { return transform_.GetActiveQuaternion(); }

   // Scale
   void SetScale(const Vector3& scale) { transform_.scale = scale; }
   const Vector3& GetScale() const { return transform_.scale; }

   void SetTransform(const Transform& transform) { transform_ = transform; }
   const Transform& GetTransform() const { return transform_; }

   /// @brief スキンメッシュ発生に使用するモデルと現在姿勢を設定する
   /// @param modelAsset 頂点・インデックスを持つモデル
   /// @param skinCluster 現在のスキニング行列。nullptrなら静的頂点を使用
   void SetSkinnedMeshSource(ModelAsset* modelAsset, const SkinCluster* skinCluster) {
	  skinnedMeshModel_ = modelAsset;
	  skinnedMeshSkinCluster_ = skinCluster;
   }

   /// @brief 形状に基づいてランダムな放出位置を取得
   Vector3 GetRandomEmissionPosition() const;

   /// @brief 形状に基づいてランダムな初期速度方向を取得
   Vector3 GetRandomEmissionDirection() const;

   /// @brief Circle 形状の中心から発生位置へ向かう外向き方向を取得
   Vector3 GetCircleOutwardDirection(const Vector3& emissionPosition) const;

   // JSON Serialization
   nlohmann::json ToJson() const;
   void FromJson(const nlohmann::json& json);

#ifdef USE_IMGUI
   void DrawInspector();
#endif

private:
   bool enabled_ = true;
   ShapeType shapeType_ = ShapeType::Cone;
   EmitFrom emitFrom_ = EmitFrom::Volume;

   float radius_ = 1.0f;
   float angle_ = 25.0f;
   float length_ = 5.0f;
   Vector3 boxSize_{ 1.0f, 1.0f, 1.0f };
   float arc_ = 360.0f;
   float circleOutwardVelocity_ = 0.0f;
   float torusMajorRadius_ = 1.0f;

   Transform transform_{};
   ModelAsset* skinnedMeshModel_ = nullptr;
   const SkinCluster* skinnedMeshSkinCluster_ = nullptr;
   mutable Vector3 lastEmissionDirection_{ 0.0f, 1.0f, 0.0f };
};
}
