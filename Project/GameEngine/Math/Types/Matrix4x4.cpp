#include "GameEngine/pch.h"
#include <cstdint>
#include "GameEngine/Math/Types/Matrix4x4.h"

namespace GameEngine {

Matrix4x4 Matrix4x4::Identity() {
   Matrix4x4 result = {};

   for (std::int32_t i = 0; i < 4; ++i) {
      result.m[i][i] = 1.0f;
   }

   return result;
}

Matrix4x4 Matrix4x4::operator+(const Matrix4x4& matrix) const {
   Matrix4x4 result;

   for (std::int32_t i = 0; i < 4; i++) {
      for (std::int32_t j = 0; j < 4; j++) {
         result.m[i][j] = m[i][j] + matrix.m[i][j];
      }
   }

   return result;
}

Matrix4x4 Matrix4x4::operator-(const Matrix4x4& matrix) const {
   Matrix4x4 result;

   for (std::int32_t i = 0; i < 4; i++) {
      for (std::int32_t j = 0; j < 4; j++) {
         result.m[i][j] = m[i][j] - matrix.m[i][j];
      }
   }

   return result;
}

Matrix4x4 Matrix4x4::operator*(const Matrix4x4& matrix) const {
   Matrix4x4 result;

   for (std::int32_t i = 0; i < 4; i++) {
      for (std::int32_t j = 0; j < 4; j++) {
         result.m[i][j] = 0;

         for (std::int32_t k = 0; k < 4; k++) {
            result.m[i][j] += m[i][k] * matrix.m[k][j];
         }
      }
   }

   return result;
}

bool Matrix4x4::operator!=(const Matrix4x4& matrix) const {
   for (std::int32_t i = 0; i < 4; i++) {
      for (std::int32_t j = 0; j < 4; j++) {
         if (m[i][j] != matrix.m[i][j]) {
            return true; // 行列が異なる場合
         }
      }
   }

   return false; // 行列が同じ場合
}

Matrix4x4 Matrix4x4::operator*=(const Matrix4x4& matrix) {
   Matrix4x4 result;

   for (std::int32_t i = 0; i < 4; i++) {
      for (std::int32_t j = 0; j < 4; j++) {
         result.m[i][j] = 0;

         for (std::int32_t k = 0; k < 4; k++) {
            result.m[i][j] += m[i][k] * matrix.m[k][j];
         }
      }
   }

   *this = result; // 元の行列を更新
   return *this;
}

Matrix4x4 Matrix4x4::Inverse() const {
   Matrix4x4 result;
   Matrix4x4 mat = *this; // オリジナルの行列を保持するためにコピーを作成

   float det = 0;

   // 第0行に沿ったLaplace展開で行列式を求め、後段の全余因子で共通利用する。
   for (std::int32_t i = 0; i < 4; ++i) {
      // 部分行列を作成
      float subMat[3][3];

      for (std::int32_t sub_i = 1; sub_i < 4; ++sub_i) {
         std::int32_t sub_j = 0;

         for (std::int32_t sub_k = 0; sub_k < 4; ++sub_k) {
            if (sub_k == i) {
               continue;
            }

            subMat[sub_i - 1][sub_j] = mat.m[sub_i][sub_k];
            ++sub_j;
         }
      }

      // 行列式を計算
      float subDet = subMat[0][0] * (subMat[1][1] * subMat[2][2] - subMat[1][2] * subMat[2][1]) -
                     subMat[0][1] * (subMat[1][0] * subMat[2][2] - subMat[1][2] * subMat[2][0]) +
                     subMat[0][2] * (subMat[1][0] * subMat[2][1] - subMat[1][1] * subMat[2][0]);
      det += (i % 2 == 0 ? 1 : -1) * mat.m[0][i] * subDet;
   }

   // 特異行列の扱いは呼び出し側の事前条件とし、ここでは分岐せず余因子法の結果をそのまま返す。

   // 各要素の余因子から随伴行列を作り、行列式で除算して逆行列を求める。
   for (std::int32_t i = 0; i < 4; ++i) {
      for (std::int32_t j = 0; j < 4; ++j) {
         // 部分行列を作成
         float subMat[3][3];

         for (std::int32_t sub_i = 0, row = 0; sub_i < 4; ++sub_i) {
            if (sub_i == i) {
               continue;
            }

            for (std::int32_t sub_j = 0, col = 0; sub_j < 4; ++sub_j) {
               if (sub_j == j) {
                  continue;
               }

               subMat[row][col] = mat.m[sub_i][sub_j];
               ++col;
            }

            ++row;
         }

         // 余因子行列の計算
         float subDet = subMat[0][0] * (subMat[1][1] * subMat[2][2] - subMat[1][2] * subMat[2][1]) -
                        subMat[0][1] * (subMat[1][0] * subMat[2][2] - subMat[1][2] * subMat[2][0]) +
                        subMat[0][2] * (subMat[1][0] * subMat[2][1] - subMat[1][1] * subMat[2][0]);

         // 余因子行列を転置して随伴行列にするため、格納先の行列添字を入れ替える。
         result.m[j][i] = ((i + j) % 2 == 0 ? 1 : -1) * subDet / det;
      }
   }

   return result;
}

Matrix4x4 Matrix4x4::Transpose() const {
   Matrix4x4 result;

   for (std::int32_t i = 0; i < 4; ++i) {
      for (std::int32_t j = 0; j < 4; ++j) {
         result.m[i][j] = m[j][i];
      }
   }

   return result;
}

} // namespace GameEngine
