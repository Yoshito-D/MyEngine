#include "pch.h"
#include "EasingFunctions.h"

namespace GameEngine {

Vector3 Easing::Slerp(const Vector3& start, const Vector3& end, float t) {
   t = std::clamp(t, 0.0f, 1.0f);
   float dot = std::clamp(start.Dot(end), -1.0f, 1.0f);
   float theta = acosf(dot);
   float sinTheta = std::sin(theta);

   if (std::abs(sinTheta) < 1e-5f) {
	  return (start * (1.0f - t) + end * t).Normalize();
   }

   return start * std::sin((1.0f - t) * theta) / sinTheta
	  + end * std::sin(t * theta) / sinTheta;
}

uint32_t Easing::LerpRGBAColor(uint32_t colorA, uint32_t colorB, float t) {
   t = std::clamp(t, 0.0f, 1.0f);

   uint8_t rA = (colorA >> 24) & 0xFF;
   uint8_t gA = (colorA >> 16) & 0xFF;
   uint8_t bA = (colorA >> 8) & 0xFF;
   uint8_t aA = colorA & 0xFF;

   uint8_t rB = (colorB >> 24) & 0xFF;
   uint8_t gB = (colorB >> 16) & 0xFF;
   uint8_t bB = (colorB >> 8) & 0xFF;
   uint8_t aB = colorB & 0xFF;

   uint8_t r = static_cast<uint8_t>(rA + (rB - rA) * t);
   uint8_t g = static_cast<uint8_t>(gA + (gB - gA) * t);
   uint8_t b = static_cast<uint8_t>(bA + (bB - bA) * t);
   uint8_t a = static_cast<uint8_t>(aA + (aB - aA) * t);

   return (r << 24) | (g << 16) | (b << 8) | a;
}

} // namespace GameEngine
