#include "pch.h"
#include "Line.h"

namespace GameEngine {
std::array<Line::Vertex, 2> Line::GetVertices() const {
   return { Vertex{ start_, color_ }, Vertex{ end_, color_ } };
}
}