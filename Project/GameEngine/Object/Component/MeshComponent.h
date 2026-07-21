#pragma once

#include "Component/IObjectComponent.h"
#include "Model/ModelAsset.h"
#include "Utility/VectorMath.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace GameEngine {
class Mesh;
class Texture;

/// @brief モデルファイルまたはエンジン標準プリミティブを選択してメッシュを供給するコンポーネント
class MeshComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "MeshComponent";
   static constexpr ComponentDisplayName kDisplayName{ "メッシュ", "Mesh" };

   /// @brief メッシュデータの入力元
   enum class SourceType {
      ModelFile = 0, ///< 外部モデルファイル
      Primitive, ///< エンジン標準プリミティブ
   };

   /// @brief プリミティブメッシュの形状タイプ
   enum class PrimitiveType {
      Quad = 0, ///< 4頂点の矩形メッシュ
      Ring, ///< 中央に穴があるリング
      Sphere, ///< UV球
      Box, ///< 直方体
      Cylinder, ///< 上下に蓋がある円柱
      Cone, ///< 円錐
      Circle, ///< 塗りつぶし円
      Plane, ///< 分割可能な平面
      Torus, ///< ドーナツ形状
      Triangle, ///< 3頂点の三角形
   };

   /// @brief 2D形状を生成する基準平面
   enum class PlaneOrientation {
      XY = 0, ///< XY平面
      XZ, ///< XZ平面
      YZ, ///< YZ平面
   };

   /// @brief コンポーネントの型名を取得する
   /// @return 型名
   const char* GetTypeName() const override;

   /// @brief メッシュ設定をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONからメッシュ設定を復元する
   /// @param data 保存済みJSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief メッシュ設定のインスペクターを描画する
   void DrawInspector() override;
#endif

   /// @brief メッシュデータの入力元を設定する
   /// @param sourceType モデルファイルまたはプリミティブ
   void SetSourceType(SourceType sourceType);

   /// @brief メッシュデータの入力元を取得する
   /// @return 現在の入力元
   SourceType GetSourceType() const { return sourceType_; }

   /// @brief 描画時にメッシュの表裏を反転するかを設定する
   /// @param reverseFaces trueなら前面判定を反転する
   void SetReverseFaces(bool reverseFaces) { reverseFaces_ = reverseFaces; }

   /// @brief 描画時にメッシュの表裏を反転するかを取得する
   /// @return 前面判定を反転する場合はtrue
   bool IsReverseFaces() const { return reverseFaces_; }

   /// @brief モデルアセットを設定し、入力元をモデルファイルへ切り替える
   /// @param modelAsset モデルアセット
   void SetModelAsset(const std::shared_ptr<ModelAsset>& modelAsset);

   /// @brief resourcesからの相対assetIdでモデルアセットを設定する
   /// @param assetId モデルアセットID。空文字列の場合は設定を解除する
   /// @return 読み込みまたは設定解除に成功した場合はtrue
   bool SetModelAssetByAssetId(const std::string& assetId);

   /// @brief 描画に使用するモデルアセットを取得する
   /// @return モデルファイルモードで読み込み済みの場合はアセット、それ以外はnullptr
   ModelAsset* GetModelAsset() const;

   /// @brief 設定されているモデルアセットIDを取得する
   /// @return resourcesからの相対アセットID
   const std::string& GetAssetId() const { return assetId_; }

   /// @brief モデルアセットハンドルを取得する
   /// @return 読み込み済みアセットの共有ハンドル
   const std::shared_ptr<ModelAsset>& GetModelAssetHandle() const { return modelAsset_; }

   /// @brief モデル単位のスキンクラスタを取得する
   /// @return モデルファイルモードで利用可能な場合はスキンクラスタ、それ以外はnullptr
   SkinCluster* GetSkinCluster();

   /// @brief モデル単位のスキンクラスタを取得する
   /// @return モデルファイルモードで利用可能な場合はスキンクラスタ、それ以外はnullptr
   const SkinCluster* GetSkinCluster() const;

   /// @brief 現在の設定で内部メッシュを作成する
   void CreateMesh();

   /// @brief 内部メッシュを必要に応じて作成して取得する
   /// @return 内部メッシュ
   Mesh* EnsureMesh();

   /// @brief 内部メッシュを取得する
   /// @return 内部メッシュ。未作成ならnullptr
   Mesh* GetMesh() const { return mesh_.get(); }

   /// @brief 生成するプリミティブ形状を設定する
   /// @param primitiveType プリミティブ形状
   void SetPrimitiveType(PrimitiveType primitiveType) {
      sourceType_ = SourceType::Primitive;
      primitiveType_ = primitiveType;
   }

   /// @brief 生成するプリミティブ形状を取得する
   /// @return プリミティブ形状
   PrimitiveType GetPrimitiveType() const { return primitiveType_; }

   /// @brief Quadの幅と高さを設定する
   /// @param size Quadサイズ
   void SetQuadSize(const Vector2& size) { quadSize_ = size; }

   /// @brief Quadの幅と高さを取得する
   /// @return Quadサイズ
   Vector2 GetQuadSize() const { return quadSize_; }

   /// @brief Quadの頂点配置用アンカーポイントを設定する
   /// @param anchorPoint 0.0から1.0の範囲を基準にした頂点配置アンカー
   void SetQuadAnchorPoint(const Vector2& anchorPoint) { quadAnchorPoint_ = anchorPoint; }

   /// @brief Quadの頂点配置用アンカーポイントを取得する
   /// @return 頂点配置アンカー
   Vector2 GetQuadAnchorPoint() const { return quadAnchorPoint_; }

   /// @brief Quadを左右反転するかを設定する
   /// @param isFlip trueなら左右反転
   void SetFlipX(bool isFlip) { flipX_ = isFlip; }

   /// @brief Quadが左右反転されているかを取得する
   /// @return 左右反転ならtrue
   bool IsFlipX() const { return flipX_; }

   /// @brief Quadを上下反転するかを設定する
   /// @param isFlip trueなら上下反転
   void SetFlipY(bool isFlip) { flipY_ = isFlip; }

   /// @brief Quadが上下反転されているかを取得する
   /// @return 上下反転ならtrue
   bool IsFlipY() const { return flipY_; }

   /// @brief 現在のプリミティブ設定をメッシュの頂点へ反映する
   void ApplyToMesh();

   /// @brief 現在のプリミティブ設定を指定メッシュの頂点へ反映する
   /// @param mesh 設定を反映するメッシュ
   void ApplyToMesh(Mesh* mesh) const;

   /// @brief テクスチャ矩形を内部メッシュのUVへ反映する
   /// @param texture UV計算に使うテクスチャ
   /// @param leftTop テクスチャ左上座標
   /// @param size テクスチャ矩形サイズ。0以下なら全体を使用する
   void ApplyTextureCoordinates(Texture* texture, const Vector2& leftTop, const Vector2& size);

private:
   SourceType sourceType_ = SourceType::ModelFile;
   bool reverseFaces_ = false;
   std::shared_ptr<ModelAsset> modelAsset_;
   std::string assetId_;
   std::optional<SkinCluster> skinCluster_;
   PrimitiveType primitiveType_ = PrimitiveType::Quad;
   PlaneOrientation planeOrientation_ = PlaneOrientation::XY;
   Vector2 quadSize_ = { 128.0f, 128.0f };
   Vector2 quadAnchorPoint_ = { 0.0f, 0.0f };
   bool flipX_ = false;
   bool flipY_ = false;
   float originY_ = 0.5f;
   float ringInnerRadius_ = 0.4f;
   float ringOuterRadius_ = 0.5f;
   uint32_t ringSegments_ = 32;
   float sphereRadius_ = 0.5f;
   uint32_t sphereStacks_ = 16;
   uint32_t sphereSlices_ = 32;
   Vector3 boxSize_ = { 1.0f, 1.0f, 1.0f };
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
   uint32_t planeWidthSegments_ = 1;
   uint32_t planeDepthSegments_ = 1;
   float torusMajorRadius_ = 0.5f;
   float torusMinorRadius_ = 0.2f;
   uint32_t torusMajorSegments_ = 32;
   uint32_t torusMinorSegments_ = 16;
   Vector3 triangleV0_ = { -0.5f, 0.0f, 0.0f };
   Vector3 triangleV1_ = { 0.0f, 1.0f, 0.0f };
   Vector3 triangleV2_ = { 0.5f, 0.0f, 0.0f };
   std::unique_ptr<Mesh> mesh_;
};
} // namespace GameEngine
