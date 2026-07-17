#pragma once

#include "Component/IObjectComponent.h"
#include "Utility/VectorMath.h"
#include <memory>

namespace GameEngine {
class Mesh;
class Texture;

/// @brief Quadなどのプリミティブメッシュ生成パラメータを保持するコンポーネント
class PrimitiveMeshComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "PrimitiveMeshComponent";
   static constexpr ComponentDisplayName kDisplayName{ "プリミティブメッシュ", "Primitive Mesh" };

   /// @brief プリミティブメッシュの形状タイプ
   enum class PrimitiveType {
      Quad ///< 4頂点の矩形メッシュ
   };

   /// @brief コンポーネントの型名を取得する
   /// @return 型名
   const char* GetTypeName() const override;

   /// @brief プリミティブメッシュ設定をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONからプリミティブメッシュ設定を復元する
   /// @param data 保存済みJSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief プリミティブメッシュ設定のインスペクターを描画する
   void DrawInspector() override;
#endif

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
   void SetPrimitiveType(PrimitiveType primitiveType) { primitiveType_ = primitiveType; }

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
   PrimitiveType primitiveType_ = PrimitiveType::Quad;
   Vector2 quadSize_ = { 128.0f, 128.0f };
   Vector2 quadAnchorPoint_ = { 0.0f, 0.0f };
   bool flipX_ = false;
   bool flipY_ = false;
   std::unique_ptr<Mesh> mesh_;
};
}
