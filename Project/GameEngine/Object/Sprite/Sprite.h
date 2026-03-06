#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "Core/Graphics/Mesh.h"
#include "Utility/VectorMath.h"
#include "Camera.h"
#include "Window.h"
#include "Object.h"

namespace GameEngine {
   class Texture;

   class Sprite :public Object {
   public:
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

	  void Create(const Vector2& size, Material* material = nullptr,const Vector2& anchorPoint = Vector2(0.0f, 0.0f));

	  void SetAnchorPoint(const Vector2& anchorPoint);

	  void SetSize(const Vector2& size);

	  void SetScale(const Vector2& scale);

	  void SetPosition(const Vector2& position);

	  void SetRotation(float rotation);

	  Vector2 GetSize() const { return size_; }
	  Vector2 GetScale() const { return Vector2{ transform_.scale.x, transform_.scale.y }; }
	  Vector2 GetPosition() const { return Vector2{ transform_.translation.x,  transform_.translation.y }; }
	  float GetRotation() const { return transform_.rotation.z; }
	  Vector2 GetAnchorPoint() const { return anchorPoint_; }

	  bool IsFlipX() const { return isFlipX_; }
	  bool IsFlipY() const { return isFlipY_; }

	  void SetFlipX(bool isFlip) { isFlipX_ = isFlip; }
	  void SetFlipY(bool isFlip) { isFlipY_ = isFlip; }

	  void SetTextureUV(const Vector2& leftTop, const Vector2& size) {
		 textureLeftTop_ = leftTop;
		 textureSize_ = size;
	  }

	  void SetTextureLeftTop(const Vector2& leftTop) {
		 textureLeftTop_ = leftTop;
	  }

	  void SetTextureSize(const Vector2& size) {
		 textureSize_ = size;
	  }

	  // テクスチャパラメータのゲッターメソッドを追加
	  Vector2 GetTextureLeftTop() const { return textureLeftTop_; }
	  Vector2 GetTextureSize() const { return textureSize_; }

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

	  Vector2 size_ = { 1.0f, 1.0f };

	  Vector2 anchorPoint_ = { 0.0f, 0.0f };

	  bool isFlipX_ = false;
	  bool isFlipY_ = false;

	  Vector2 textureLeftTop_ = { 0.0f, 0.0f };
	  Vector2 textureSize_ = { 0.0f, 0.0f };  // 0に設定することで自動検出をトリガー

   private:

	  /// @brief アンカーポイントに基づいて画面座標を計算
	  /// @param anchorPoint アンカーポイント
	  /// @param screenWidth 画面幅
	  /// @param screenHeight 画面高さ
	  /// @return 調整された座標
	  Vector3 CalculateAnchorPosition(AnchorPoint anchorPoint, uint32_t screenWidth, uint32_t screenHeight) const;

	  void UpdateVertexPositions();

	  void UpdateTextureCoordinates(Texture* texture);
   };
}