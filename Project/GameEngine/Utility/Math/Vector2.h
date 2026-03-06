#pragma once
#include <cmath>

namespace GameEngine {

struct Vector2 {
   float x;
   float y;

   Vector2 operator+(const Vector2& other) const { return { x + other.x, y + other.y }; }
   Vector2 operator-(const Vector2& other) const { return { x - other.x, y - other.y }; }
   Vector2 operator*(float scalar) const { return { x * scalar, y * scalar }; }
   Vector2 operator/(float scalar) const { if (scalar != 0) { return { x / scalar, y / scalar }; } }
   Vector2 operator+= (const Vector2& other) { x += other.x; y += other.y; return *this; }
   Vector2 operator*= (float scalar) { x *= scalar; y *= scalar; return *this; }

   Vector2 Normalize() const {
	  float length = std::sqrt(x * x + y * y);
	  if (length == 0) {
		 return { 0, 0 };
	  }
	  return { x / length, y / length };
   }

   float Length() const {
	  return std::sqrt(x * x + y * y);
   }
};

} // namespace GameEngine