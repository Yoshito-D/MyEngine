#pragma once

namespace GameEngine {

/// @brief 3行3列の浮動小数点行列。
/// @details 2次元の同次変換や3次元の線形変換を保持するための値型として使用する。
struct Matrix3x3 {
   float m[3][3]; //!< 行、列の順にアクセスする行列要素（m[row][column]）
};

} // namespace GameEngine