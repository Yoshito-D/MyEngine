#pragma once
#include "Utility/VectorMath.h"
#include "Camera.h"
#include "Window.h"
#include "Object.h"
#include <vector>

namespace GameEngine {
class Material;
class MaterialComponent;
class Mesh;
class PrimitiveMeshComponent;
class TransformationMatrix;
class TransformComponent;
class Texture;

class Sprite :public Object {
public:
   Sprite();
   ~Sprite() override;

   static const std::vector<Sprite*>& GetRegisteredSprites();

   static void UnregisterSprite(Sprite* sprite);

   static void ClearRegisteredSprites() { sRegisteredSprites_.clear(); }

   /// @brief UI描画用のアンカーポイント
   enum class AnchorPoint {
	  TopLeft,      // 左上
	  TopCenter,    // 上中央
	  TopRight,     // 右上
	  MiddleLeft,   // 左中央
	  MiddleCenter, // 中央
	  MiddleRight,  // 右中央
	  BottomLeft,   // 左下
	  BottomCenter, // 下中央
	  BottomRight   // 右下
   };

   void Create(const Vector2& size = Vector2(128.0f, 128.0f), Material* material = nullptr, const Vector2& anchorPoint = Vector2(0.0f, 0.0f));

   /// @brief Quad頂点配置用アンカーを設定する
   /// @param anchorPoint 0.0から1.0の範囲を基準にした頂点配置アンカー
   void SetAnchorPoint(const Vector2& anchorPoint);

   /// @brief SpriteのQuadサイズを設定する
   /// @param size Quadサイズ
   void SetSize(const Vector2& size);

   void SetScale(const Vector2& scale);

   void SetPosition(const Vector2& position);

   void SetRotation(float rotation);

   /// @brief SpriteのQuadサイズを取得する
   /// @return Quadサイズ
   Vector2 GetSize() const;
   Vector2 GetScale() const;
   Vector2 GetPosition() const;
   float GetRotation() const;
   /// @brief Quad頂点配置用アンカーを取得する
   /// @return 頂点配置アンカー
   Vector2 GetAnchorPoint() const;

   /// @brief 自動スクリーン描画で使用する画面アンカーを設定する
   /// @param screenAnchorPoint 画面上の配置基準
   void SetScreenAnchorPoint(AnchorPoint screenAnchorPoint) { screenAnchorPoint_ = screenAnchorPoint; }

   /// @brief 自動スクリーン描画で使用する画面アンカーを取得する
   /// @return 画面上の配置基準
   AnchorPoint GetScreenAnchorPoint() const { return screenAnchorPoint_; }

   /// @brief SpriteのQuadが左右反転されているかを取得する
   /// @return 左右反転ならtrue
   bool IsFlipX() const;
   /// @brief SpriteのQuadが上下反転されているかを取得する
   /// @return 上下反転ならtrue
   bool IsFlipY() const;

   /// @brief SpriteのQuadを左右反転するかを設定する
   /// @param isFlip trueなら左右反転
   void SetFlipX(bool isFlip);
   /// @brief SpriteのQuadを上下反転するかを設定する
   /// @param isFlip trueなら上下反転
   void SetFlipY(bool isFlip);

   /// @brief 使用するテクスチャ矩形をピクセル座標で設定する
   /// @param leftTop テクスチャ左上座標
   /// @param size テクスチャ矩形サイズ。0以下なら全体を使用する
   void SetTextureUV(const Vector2& leftTop, const Vector2& size);

   /// @brief 使用するテクスチャ矩形の左上座標を設定する
   /// @param leftTop テクスチャ左上座標
   void SetTextureLeftTop(const Vector2& leftTop);

   /// @brief 使用するテクスチャ矩形のサイズを設定する
   /// @param size テクスチャ矩形サイズ。0以下なら全体を使用する
   void SetTextureSize(const Vector2& size);

   /// @brief Sprite描画で使用するメッシュを取得する
   /// @return PrimitiveMeshComponentが所有するメッシュ
   Mesh* GetMesh() const;

   /// @brief トランスフォーメーションマトリックスを取得
   TransformationMatrix* GetTransformationMatrix();

   /// @brief 使用するテクスチャ矩形の左上座標を取得する
   /// @return テクスチャ左上座標
   Vector2 GetTextureLeftTop() const;
   /// @brief 使用するテクスチャ矩形のサイズを取得する
   /// @return テクスチャ矩形サイズ
   Vector2 GetTextureSize() const;

   /// @brief 通常描画用の行列更新（ワールド座標で処理）
   /// @param camera カメラ
   /// @details Renderer::Draw()で使用。transform_.translationはワールド座標として扱われる
   void Update(Camera* camera, Texture* texture);

   /// @brief UI描画用の行列更新（テクスチャ付き）
   /// @param camera カメラ
   /// @param texture テクスチャ
   /// @param anchorPoint アンカーポイント
   /// @param screenWidth 画面幅
   /// @param screenHeight 画面高さ
   /// @details Renderer::DrawUI()で使用。transform_.translationはスクリーン座標のオフセットとして扱われる
   void UpdateMatrixForUI(Camera* camera, Texture* texture, AnchorPoint anchorPoint = AnchorPoint::TopLeft, uint32_t screenWidth = Window::kResolutionWidth, uint32_t screenHeight = Window::kResolutionHeight);
private:
   AnchorPoint screenAnchorPoint_ = AnchorPoint::MiddleCenter;

private:

   /// @brief アンカーポイントに基づいて画面座標を計算
   /// @param anchorPoint アンカーポイント
   /// @param screenWidth 画面幅
   /// @param screenHeight 画面高さ
   /// @return 調整された座標
   Vector3 CalculateAnchorPosition(AnchorPoint anchorPoint, uint32_t screenWidth, uint32_t screenHeight) const;

   void UpdateVertexPositions();

   void UpdateTextureCoordinates(Texture* texture);

   PrimitiveMeshComponent* GetPrimitiveMeshComponent();
   const PrimitiveMeshComponent* GetPrimitiveMeshComponent() const;
   MaterialComponent* GetMaterialComponent();
   const MaterialComponent* GetMaterialComponent() const;
   TransformComponent* GetTransformComponent();
   const TransformComponent* GetTransformComponent() const;

private:
   inline static std::vector<Sprite*> sRegisteredSprites_{};
};
}
