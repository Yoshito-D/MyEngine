#include "GameEngine/pch.h"
#include "GameEngine/Math/Types/Vector2.h"

namespace GameEngine {

Vector2 Vector2::operator/(float scalar) const {
   // 0除算でも未定義の戻り値を呼び出し元へ渡さない。
   if (scalar == 0.0f) {
      return {0.0f, 0.0f};
   }

   return {x / scalar, y / scalar};
}

Vector2 Vector2::operator+=(const Vector2& other) {
   x += other.x;
   y += other.y;
   return *this;
}

Vector2 Vector2::operator*=(float scalar) {
   x *= scalar;
   y *= scalar;
   return *this;
}

Vector2 Vector2::Normalize() const {
   float length = std::sqrt(x * x + y * y);

   // 0ベクトルは向きを定義できず除算もできないため、安全な0ベクトルとして扱う。
   if (length == 0) {
      return {0, 0};
   }

   return {x / length, y / length};
}

} // namespace GameEngine
