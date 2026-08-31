#pragma once
#include <DirectXMath.h>
#include "Vector4.h"

using namespace DirectX;

namespace GameEngine {

/// @brief 4行4列の浮動小数点行列。
/// @details 要素はm[row][column]で保持する。MatrixOperationsが生成する変換行列と、TransformCoordinate、TransformVectorByMatrix、TransformNormalは行ベクトル規約を採用し、平行移動をm[3][0..2]へ格納する。
/// @note Vector4::operator*(Matrix4x4)と、それを使うTransformPositionは行列の各行との内積を取る別の要素規約である。必要な規約に対応する変換APIを選択すること。
struct Matrix4x4 {
   float m[4][4]; //!< 行、列の順にアクセスする行列要素

   /// @brief 乗算しても座標や行列を変化させない単位行列を生成する。
   /// @return 対角成分が1、それ以外が0の4x4行列。
   static Matrix4x4 Identity() {
	  Matrix4x4 result = {};
	  for (int i = 0; i < 4; ++i) {
		 result.m[i][i] = 1.0f;
	  }
	  return result;
   }

   /// @brief 2つの行列を要素ごとに加算する。
   /// @param matrix 加算する行列。
   /// @return result.m[i][j] = m[i][j] + matrix.m[i][j]で求めた行列。
   Matrix4x4 operator+(const Matrix4x4& matrix) const {
	  Matrix4x4 result;
	  for (int i = 0; i < 4; i++) {
		 for (int j = 0; j < 4; j++) {
			result.m[i][j] = m[i][j] + matrix.m[i][j];
		 }
	  }
	  return result;
   }

   /// @brief 2つの行列を要素ごとに減算する。
   /// @param matrix 減算する行列。
   /// @return result.m[i][j] = m[i][j] - matrix.m[i][j]で求めた行列。
   Matrix4x4 operator-(const Matrix4x4& matrix) const {
	  Matrix4x4 result;
	  for (int i = 0; i < 4; i++) {
		 for (int j = 0; j < 4; j++) {
			result.m[i][j] = m[i][j] - matrix.m[i][j];
		 }
	  }
	  return result;
   }

   /// @brief この行列とmatrixの行列積を求める。
   /// @param matrix 右側から乗算する行列。
   /// @return result.m[i][j] = sum(m[i][k] * matrix.m[k][j])で求めた積。
   /// @details エンジンの行ベクトル規約では、変換A * BはベクトルへA、Bの順で適用される。行列積は交換可能ではない。
   Matrix4x4 operator*(const Matrix4x4& matrix) const {
	  Matrix4x4 result;
	  for (int i = 0; i < 4; i++) {
		 for (int j = 0; j < 4; j++) {
			result.m[i][j] = 0;
			for (int k = 0; k < 4; k++) {
			   result.m[i][j] += m[i][k] * matrix.m[k][j];
			}
		 }
	  }
	  return result;
   }

   /// @brief いずれかの対応要素が異なるかを厳密比較する。
   /// @param matrix 比較する行列。
   /// @return 1要素でもoperator!=による不一致があればtrue、全要素が一致すればfalse。
   /// @note 浮動小数点誤差の許容幅は設けないため、計算結果の近似比較には適さない。
   bool operator!=(const Matrix4x4& matrix) const {
	  for (int i = 0; i < 4; i++) {
		 for (int j = 0; j < 4; j++) {
			if (m[i][j] != matrix.m[i][j]) {
			   return true; // 行列が異なる場合
			}
		 }
	  }
	  return false; // 行列が同じ場合
   }

   /// @brief 右側からmatrixを乗算し、この行列を更新する。
   /// @param matrix 右側から乗算する行列。
   /// @return 更新後の行列のコピー。
   /// @details 右辺が*this自身でも途中結果を壊さないよう、一時行列へ積を完成させてから代入する。
   /// @note 一般的な複合代入演算子と異なり、戻り値は参照ではない。
   Matrix4x4 operator*=(const Matrix4x4& matrix) {
	  Matrix4x4 result;
	  for (int i = 0; i < 4; i++) {
		 for (int j = 0; j < 4; j++) {
			result.m[i][j] = 0;
			for (int k = 0; k < 4; k++) {
			   result.m[i][j] += m[i][k] * matrix.m[k][j];
			}
		 }
	  }
	  *this = result; // 元の行列を更新
	  return *this;
   }

   /// @brief 余因子と随伴行列を用いて逆行列を求める。
   /// @return この行列の逆行列。
   /// @pre 行列式が0でないこと。関数内では特異行列を検査せず、0除算時は無限大やNaNを含む可能性がある。
   /// @note 行列式が0に近い行列では、float精度の誤差が大きく増幅される。
   Matrix4x4 Inverse() const {
	  Matrix4x4 result;
	  Matrix4x4 mat = *this; // オリジナルの行列を保持するためにコピーを作成

	  float det = 0;

	  // 第0行に沿ったLaplace展開で行列式を求め、後段の全余因子で共通利用する。
	  for (int i = 0; i < 4; ++i) {
		 // 部分行列を作成
		 float subMat[3][3];
		 for (int sub_i = 1; sub_i < 4; ++sub_i) {
			int sub_j = 0;
			for (int sub_k = 0; sub_k < 4; ++sub_k) {
			   if (sub_k == i) continue;
			   subMat[sub_i - 1][sub_j] = mat.m[sub_i][sub_k];
			   ++sub_j;
			}
		 }

		 // 行列式を計算
		 float subDet =
			subMat[0][0] * (subMat[1][1] * subMat[2][2] - subMat[1][2] * subMat[2][1])
			- subMat[0][1] * (subMat[1][0] * subMat[2][2] - subMat[1][2] * subMat[2][0])
			+ subMat[0][2] * (subMat[1][0] * subMat[2][1] - subMat[1][1] * subMat[2][0]);
		 det += (i % 2 == 0 ? 1 : -1) * mat.m[0][i] * subDet;
	  }

	  // 特異行列の扱いは呼び出し側の事前条件とし、ここでは分岐せず余因子法の結果をそのまま返す。

	  // 各要素の余因子から随伴行列を作り、行列式で除算して逆行列を求める。
	  for (int i = 0; i < 4; ++i) {
		 for (int j = 0; j < 4; ++j) {
			// 部分行列を作成
			float subMat[3][3];
			for (int sub_i = 0, row = 0; sub_i < 4; ++sub_i) {
			   if (sub_i == i) continue;
			   for (int sub_j = 0, col = 0; sub_j < 4; ++sub_j) {
				  if (sub_j == j) continue;
				  subMat[row][col] = mat.m[sub_i][sub_j];
				  ++col;
			   }
			   ++row;
			}

			// 余因子行列の計算
			float subDet =
			   subMat[0][0] * (subMat[1][1] * subMat[2][2] - subMat[1][2] * subMat[2][1])
			   - subMat[0][1] * (subMat[1][0] * subMat[2][2] - subMat[1][2] * subMat[2][0])
			   + subMat[0][2] * (subMat[1][0] * subMat[2][1] - subMat[1][1] * subMat[2][0]);

			// 余因子行列を転置して随伴行列にするため、格納先の行列添字を入れ替える。
			result.m[j][i] = ((i + j) % 2 == 0 ? 1 : -1) * subDet / det;
		 }
	  }

	  return result;
   }

   /// @brief 行と列を入れ替えた転置行列を求める。
   /// @return result.m[i][j] = m[j][i]で求めた行列。
   Matrix4x4 Transpose() const {
	  Matrix4x4 result;
	  for (int i = 0; i < 4; ++i) {
		 for (int j = 0; j < 4; ++j) {
			result.m[i][j] = m[j][i];
		 }
	  }
	  return result;
   }
};

} // namespace GameEngine