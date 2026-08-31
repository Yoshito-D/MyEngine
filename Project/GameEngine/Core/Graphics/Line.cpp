#include "pch.h"
#include "Line.h"

namespace GameEngine {
/// @brief 線分をLINELIST描画用の2頂点へ展開する。
/// @details 配列順は始点、終点で固定し、両端へ同じ線色を設定する。これにより描画側はLineの
///          内部表現へ依存せず、そのまま1本分の頂点データとして扱える。
/// @return 始点と終点を描画順に格納した固定長配列
std::array<Line::Vertex, 2> Line::GetVertices() const {
   return { Vertex{ start_, color_ }, Vertex{ end_, color_ } };
}
}