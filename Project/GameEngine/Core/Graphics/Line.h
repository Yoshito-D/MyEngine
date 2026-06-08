#pragma once
#include "../Utility/VectorMath.h"
#include <array>

namespace GameEngine {

/// @brief 線分クラス
class Line {
public:

   /// @brief 頂点構造体
   struct Vertex {
	  Vector3 position;
	  Vector4 color;
   };

   /// @brief コンストラクタ
   /// @param start スタート位置
   /// @param end エンド位置
   /// @param color 線の色
   Line(const Vector3& start, const Vector3& end, const Vector4& color)
	  : start_(start), end_(end), color_(color) {}

   /// @brief デストラクタ
   ~Line() = default;

   /// @brief スタート位置の取得
   /// @return スタート位置
   const Vector3& GetStart() const { return start_; }

   /// @brief エンド位置の取得
   /// @return エンド位置
   const Vector3& GetEnd() const { return end_; }

   /// @brief 色の取得
   /// @return 色
   const Vector4& GetColor() const { return color_; }

   /// @brief 頂点情報の取得
   /// @return 頂点情報の配列
   std::array<Vertex, 2> GetVertices() const;

private:
   Vector3 start_;
   Vector3 end_;
   Vector4 color_;
};
}
