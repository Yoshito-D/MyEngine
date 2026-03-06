#pragma once
#include "../VectorMath.h"
#include <cstdint>

namespace GameEngine {

/// @brief 32ビット整数をRGBAカラーに変換
/// @param color 32ビット整数カラー
/// @return 変換後のRGBAカラー
Vector4 ConvertUIntToColor(uint32_t color);

/// @brief 2つのRGBAカラーをランダムに線形補間
/// @param colorA 最初のRGBAカラー
/// @param colorB 2番目のRGBAカラー
/// @return 線形補間されたRGBAカラー
uint32_t RandomLerpRGBAColor(uint32_t colorA, uint32_t colorB);

/// @brief HSV色空間からRGB色空間に変換
/// @param h 色相（0〜360度）
/// @param s 彩度（0.0〜1.0）
/// @param v 明度（0.0〜1.0）
/// @param outR 出力の赤成分（0〜255）
/// @param outG 出力の緑成分（0〜255）
/// @param outB 出力の青成分（0〜255）
void HSVtoRGB(float h, float s, float v, uint8_t& outR, uint8_t& outG, uint8_t& outB);

/// @brief 色相環を回る色を返す（t: 0〜1でループ）
/// @param t 色相環の位置（0.0〜1.0）
/// @param saturation 彩度（0.0〜1.0）
/// @param value 明度（0.0〜1.0）
/// @param alpha アルファ値（0〜255）
/// @return 色相環を回る色（ARGB形式の32ビット整数）
uint32_t GetLoopingHueColor(float t, float saturation = 1.0f, float value = 1.0f, uint8_t alpha = 255);

} // namespace GameEngine
