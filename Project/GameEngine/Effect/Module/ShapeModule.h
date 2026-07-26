#pragma once
#include "Utility/VectorMath.h"
#include "Utility/Math/Transform.h"
#include <nlohmann/json.hpp>
#include "ParticleModule.h"
#include <algorithm>

namespace GameEngine {
class ModelAsset;
struct SkinCluster;
/// @brief パーティクルの発生位置と初期方向を幾何形状から生成する
class ShapeModule {
public:
   /// @brief 放出元として利用できる幾何形状
   enum class ShapeType {
	  Sphere,      ///< 球
	  Hemisphere,  ///< 半球
	  Cone,        ///< 円錐
	  Box,         ///< 箱
	  Circle,      ///< 円
	  Edge,        ///< 線分
	  Point,       ///< 原点
	  Cylinder,    ///< 円柱。既存JSONとの互換性維持のため列挙末尾側に配置
	  Torus,       ///< トーラス
	  SkinnedMesh  ///< 現在姿勢のスキニング済みメッシュ表面
   };

   /// @brief 形状のどの領域から放出するかを指定する
   enum class EmitFrom {
	  Volume,      ///< 体積全体
	  Shell,       ///< 表面のみ
	  Edge         ///< 輪郭または辺のみ
   };

   /// @brief 円錐形状を使う既定設定で構築する
   ShapeModule();

   /// @brief 形状による初期位置・方向の生成を有効化する
   /// @param enabled 使用する場合はtrue
   void SetEnabled(bool enabled) { enabled_ = enabled; }
   /// @brief 形状による生成が有効か取得する
   /// @return 有効な場合はtrue
   bool IsEnabled() const { return enabled_; }

   /// @brief 放出元の幾何形状を設定する
   /// @param type 使用する形状
   void SetShapeType(ShapeType type) { shapeType_ = type; }
   /// @brief 放出元の幾何形状を取得する
   /// @return 現在の形状
   ShapeType GetShapeType() const { return shapeType_; }

   /// @brief 形状内の放出領域を設定する
   /// @param from 体積・表面・輪郭のいずれか
   void SetEmitFrom(EmitFrom from) { emitFrom_ = from; }
   /// @brief 形状内の放出領域を取得する
   /// @return 現在の放出領域
   EmitFrom GetEmitFrom() const { return emitFrom_; }

   /// @brief 球・半球・円・円錐・円柱の半径を設定する
   /// @param radius 形状半径
   void SetRadius(float radius) { radius_ = radius; }
   /// @brief 球系形状の半径を取得する
   /// @return 形状半径
   float GetRadius() const { return radius_; }

   /// @brief 円錐の開き角を設定する
   /// @param angle 円錐角（度）
   void SetAngle(float angle) { angle_ = angle; }
   /// @brief 円錐の開き角を取得する
   /// @return 円錐角（度）
   float GetAngle() const { return angle_; }
   /// @brief 円錐・円柱・線分の軸方向長さを設定する
   /// @param length 形状の長さ
   void SetLength(float length) { length_ = length; }
   /// @brief 軸方向の長さを取得する
   /// @return 形状の長さ
   float GetLength() const { return length_; }

   /// @brief 箱形状の各軸サイズを設定する
   /// @param size 幅・高さ・奥行き
   void SetBoxSize(const Vector3& size) { boxSize_ = size; }
   /// @brief 箱形状の各軸サイズを取得する
   /// @return 幅・高さ・奥行き
   const Vector3& GetBoxSize() const { return boxSize_; }

   /// @brief 円・円錐の放出に使う円弧範囲を設定する
   /// @param arc 0から360までの角度範囲（度）
   void SetArc(float arc) { arc_ = arc; }
   /// @brief 円弧範囲を取得する
   /// @return 角度範囲（度）
   float GetArc() const { return arc_; }
   /// @brief 円形状で中心から外向きに加える速度を設定する
   /// @param velocity 外向き速度
   void SetCircleOutwardVelocity(float velocity) { circleOutwardVelocity_ = velocity; }
   /// @brief 円形状の外向き速度を取得する
   /// @return 外向き速度
   float GetCircleOutwardVelocity() const { return circleOutwardVelocity_; }

   /// @brief トーラスの主半径を設定する
   /// @param radius チューブ中心からトーラス中心までの0以上の距離
   void SetTorusMajorRadius(float radius) { torusMajorRadius_ = std::max(radius, 0.0f); }

   /// @brief トーラスの主半径を取得する
   /// @return チューブ中心からトーラス中心までの距離
   float GetTorusMajorRadius() const { return torusMajorRadius_; }

   /// @brief 形状のローカル位置を設定する
   /// @param position エミッター基準の位置
   void SetPosition(const Vector3& position) { transform_.translation = position; }
   /// @brief 形状のローカル位置を取得する
   /// @return エミッター基準の位置
   const Vector3& GetPosition() const { return transform_.translation; }

   /// @brief 形状のローカル回転を設定する
   /// @param rotation エミッター基準の回転
   void SetRotation(const Quaternion& rotation) { transform_.SetRotationQuaternion(rotation); }
   /// @brief 形状のローカル回転をQuaternionで取得する
   /// @return エミッター基準の回転
   Quaternion GetRotationQuaternion() const { return transform_.GetActiveQuaternion(); }

   /// @brief 形状のローカルスケールを設定する
   /// @param scale 各軸の拡縮率
   void SetScale(const Vector3& scale) { transform_.scale = scale; }
   /// @brief 形状のローカルスケールを取得する
   /// @return 各軸の拡縮率
   const Vector3& GetScale() const { return transform_.scale; }

   /// @brief 形状の位置・回転・スケールをまとめて設定する
   /// @param transform エミッター基準のローカルトランスフォーム
   void SetTransform(const Transform& transform) { transform_ = transform; }
   /// @brief 形状のローカルトランスフォームを取得する
   /// @return エミッター基準のトランスフォーム
   const Transform& GetTransform() const { return transform_; }

   /// @brief スキンメッシュ発生に使用するモデルと現在姿勢を設定する
   /// @param modelAsset 頂点・インデックスを持つモデル
   /// @param skinCluster 現在のスキニング行列。nullptrなら静的頂点を使用
   void SetSkinnedMeshSource(ModelAsset* modelAsset, const SkinCluster* skinCluster) {
	  skinnedMeshModel_ = modelAsset;
	  skinnedMeshSkinCluster_ = skinCluster;
   }

   /// @brief 形状に基づいてランダムな放出位置を取得
   /// @return 形状のローカル変換を反映した放出位置
   Vector3 GetRandomEmissionPosition() const;

   /// @brief 形状に基づいてランダムな初期速度方向を取得
   /// @return 正規化された放出方向
   Vector3 GetRandomEmissionDirection() const;

   /// @brief Circle 形状の中心から発生位置へ向かう外向き方向を取得
   /// @param emissionPosition GetRandomEmissionPositionが返した位置
   /// @return 中心から外側を向く正規化方向
   Vector3 GetCircleOutwardDirection(const Vector3& emissionPosition) const;

   /// @brief 形状設定をJSONへ変換する
   /// @return 現在の全設定を含むJSON
   nlohmann::json ToJson() const;
   /// @brief JSONに含まれる形状設定を反映する
   /// @param json 読み込む設定
   void FromJson(const nlohmann::json& json);

#ifdef USE_IMGUI
   /// @brief 形状設定を編集するインスペクターを描画する
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
