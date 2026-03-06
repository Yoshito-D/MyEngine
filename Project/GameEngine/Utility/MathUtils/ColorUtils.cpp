#include "pch.h"
#include "ColorUtils.h"
#include <random>

namespace GameEngine {

Vector4 ConvertUIntToColor(uint32_t color) {
   float r = ((color >> 24) & 0xFF) / 255.0f;
   float g = ((color >> 16) & 0xFF) / 255.0f;
   float b = ((color >> 8) & 0xFF) / 255.0f;
   float a = (color & 0xFF) / 255.0f;
   return Vector4(r, g, b, a);
}

uint32_t RandomLerpRGBAColor(uint32_t colorA, uint32_t colorB) {
   static std::mt19937 rng(std::random_device{}());
   std::uniform_real_distribution<float> dist(0.0f, 1.0f);

   float t = dist(rng);

   // 各チャンネルを抽出
   uint8_t rA = (colorA >> 24) & 0xFF;
   uint8_t gA = (colorA >> 16) & 0xFF;
   uint8_t bA = (colorA >> 8) & 0xFF;
   uint8_t aA = colorA & 0xFF;

   uint8_t rB = (colorB >> 24) & 0xFF;
   uint8_t gB = (colorB >> 16) & 0xFF;
   uint8_t bB = (colorB >> 8) & 0xFF;
   uint8_t aB = colorB & 0xFF;

   // 線形補間
   uint8_t r = static_cast<uint8_t>(rA + (rB - rA) * t);
   uint8_t g = static_cast<uint8_t>(gA + (gB - gA) * t);
   uint8_t b = static_cast<uint8_t>(bA + (bB - bA) * t);
   uint8_t a = static_cast<uint8_t>(aA + (aB - aA) * t);

   return (r << 24) | (g << 16) | (b << 8) | a;
}

void HSVtoRGB(float h, float s, float v, uint8_t& outR, uint8_t& outG, uint8_t& outB) {
   float c = v * s;
   float x = c * (1.0f - std::fabs(fmod(h / 60.0f, 2.0f) - 1.0f));
   float m = v - c;

   float r, g, b;
   if (h < 60.0f) {
	  r = c; g = x; b = 0;
   } else if (h < 120.0f) {
	  r = x; g = c; b = 0;
   } else if (h < 180.0f) {
	  r = 0; g = c; b = x;
   } else if (h < 240.0f) {
	  r = 0; g = x; b = c;
   } else if (h < 300.0f) {
	  r = x; g = 0; b = c;
   } else {
	  r = c; g = 0; b = x;
   }

   outR = static_cast<uint8_t>((r + m) * 255.0f);
   outG = static_cast<uint8_t>((g + m) * 255.0f);
   outB = static_cast<uint8_t>((b + m) * 255.0f);
}

uint32_t GetLoopingHueColor(float t, float saturation, float value, uint8_t alpha) {
   t = fmod(t, 1.0f); // 0〜1 にループ
   if (t < 0.0f) t += 1.0f;

   float hue = t * 360.0f; // 色相に変換（0〜360°）

   uint8_t r, g, b;
   HSVtoRGB(hue, saturation, value, r, g, b);

   return (r << 24) | (g << 16) | (b << 8) | alpha;
}

} // namespace GameEngine
