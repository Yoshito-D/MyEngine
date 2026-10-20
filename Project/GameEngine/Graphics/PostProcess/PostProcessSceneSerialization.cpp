#include "GameEngine/pch.h"
#include "GameEngine/Utility/JsonValue.h"
#include "GameEngine/Graphics/PostProcess/AntiAliasing.h"
#include "GameEngine/Graphics/PostProcess/Bloom.h"
#include "GameEngine/Graphics/PostProcess/BoxFilter.h"
#include "GameEngine/Graphics/PostProcess/ChromaticAberration.h"
#include "GameEngine/Graphics/PostProcess/Dissolve.h"
#include "GameEngine/Graphics/PostProcess/GaussFilter.h"
#include "GameEngine/Graphics/PostProcess/Outline.h"
#include "GameEngine/Graphics/PostProcess/Pixelation.h"
#include "GameEngine/Graphics/PostProcess/RadialBlur.h"
#include "GameEngine/Graphics/PostProcess/ShockWave.h"
#include "GameEngine/Graphics/PostProcess/SpeedLine.h"
#include "GameEngine/Graphics/PostProcess/Vignette.h"
#include "GameEngine/Graphics/PostProcess/WhiteNoise.h"
#include <cmath>
#include <nlohmann/json.hpp>

namespace {
using json = nlohmann::json;

// Component設定は手編集された部分設定も読み込むため、
// 各値は「キーが有効なら採用し、それ以外は現在値を維持する」という規則で統一する。
// 例外をこの境界で吸収しておくことで、1 項目の不正値がエフェクトスタック全体の復元を妨げない。

int32_t ReadInt32(const json& settings, const char* key, int32_t fallback) {
   const auto it = settings.find(key);

   if (it == settings.end() || !it->is_number_integer()) {
      return fallback;
   }

   try {
      // JSON の整数が int32_t の表現範囲を超える場合も、既存設定を壊さずに維持する。
      return it->get<int32_t>();
   } catch (const json::exception&) {
      return fallback;
   }
}

std::string ReadString(const json& settings, const char* key, const std::string& fallback) {
   const auto it = settings.find(key);
   return (it != settings.end() && it->is_string()) ? it->get<std::string>() : fallback;
}

GameEngine::Vector2 ReadVector2(const json& settings, const char* key, const GameEngine::Vector2& fallback) {
   const auto it = settings.find(key);

   if (it == settings.end() || !it->is_array() || it->size() != 2 || !(*it)[0].is_number() || !(*it)[1].is_number()) {
      return fallback;
   }

   try {
      const GameEngine::Vector2 value((*it)[0].get<float>(), (*it)[1].get<float>());
      return (std::isfinite(value.x) && std::isfinite(value.y)) ? value : fallback;
   } catch (const json::exception&) {
      return fallback;
   }
}

void ReadFloat4(const json& settings, const char* key, float (&values)[4]) {
   const auto it = settings.find(key);

   if (it == settings.end() || !it->is_array() || it->size() != 4) return;

   for (size_t index = 0; index < 4; ++index) {
      if (!(*it)[index].is_number()) return;
   }

   // 4要素を一時配列へすべて検証してから反映し、途中要素だけ更新された色を作らない。
   try {
      float parsedValues[4]{};

      for (size_t index = 0; index < 4; ++index) {
         parsedValues[index] = (*it)[index].get<float>();

         if (!std::isfinite(parsedValues[index])) return;
      }

      for (size_t index = 0; index < 4; ++index) {
         values[index] = parsedValues[index];
      }
   } catch (const json::exception&) {
      return;
   }
}
} // namespace

namespace GameEngine {

// 各エフェクトの Serialize／Deserialize は同じキー名を対にして管理する。
// Deserialize は全フィールドを必須にせず、未指定の項目は現在値を維持して部分設定を適用する。
// 値の反映後は GPU 定数バッファも同期し、ロード直後の描画から復元結果が見える状態にする。
nlohmann::json AntiAliasing::SerializeSettings() const {
   return {{"contrastThreshold", contrastThreshold_}, {"relativeThreshold", relativeThreshold_},
      {"subpixelBlending", subpixelBlending_}, {"edgeSearchSteps", edgeSearchSteps_}};
}

bool AntiAliasing::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }

   contrastThreshold_ = GameEngine::JsonValue::ReadFloat(settings, "contrastThreshold", contrastThreshold_);
   relativeThreshold_ = GameEngine::JsonValue::ReadFloat(settings, "relativeThreshold", relativeThreshold_);
   subpixelBlending_ = GameEngine::JsonValue::ReadFloat(settings, "subpixelBlending", subpixelBlending_);
   edgeSearchSteps_ = GameEngine::JsonValue::ReadFloat(settings, "edgeSearchSteps", edgeSearchSteps_);
   UpdateConstantBuffer();
   return true;
}

nlohmann::json Bloom::SerializeSettings() const {
   return {{"threshold", threshold_}, {"softThreshold", softThreshold_}, {"intensity", intensity_},
      {"blurRadius", blurRadius_}};
}

bool Bloom::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }

   threshold_ = GameEngine::JsonValue::ReadFloat(settings, "threshold", threshold_);
   softThreshold_ = GameEngine::JsonValue::ReadFloat(settings, "softThreshold", softThreshold_);
   intensity_ = GameEngine::JsonValue::ReadFloat(settings, "intensity", intensity_);
   blurRadius_ = GameEngine::JsonValue::ReadFloat(settings, "blurRadius", blurRadius_);
   UpdateConstantBuffer();
   return true;
}

nlohmann::json BoxFilter::SerializeSettings() const {
   return {{"kernelRadius", kernelRadius_}};
}

bool BoxFilter::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }

   kernelRadius_ = ReadInt32(settings, "kernelRadius", kernelRadius_);
   UpdateConstantBuffer();
   return true;
}

nlohmann::json ChromaticAberration::SerializeSettings() const {
   return {{"center", {centerX_, centerY_}}, {"pixelShift", pixelShift_},
      {"useFixedDirection", useFixedDirection_ != 0}, {"fixedDirection", {fixedDirectionX_, fixedDirectionY_}}};
}

bool ChromaticAberration::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }

   const Vector2 center = ReadVector2(settings, "center", Vector2(centerX_, centerY_));
   const Vector2 fixedDirection = ReadVector2(settings, "fixedDirection", Vector2(fixedDirectionX_, fixedDirectionY_));
   centerX_ = center.x;
   centerY_ = center.y;
   pixelShift_ = GameEngine::JsonValue::ReadFloat(settings, "pixelShift", pixelShift_);
   useFixedDirection_ = GameEngine::JsonValue::ReadBool(settings, "useFixedDirection", useFixedDirection_ != 0) ? 1 : 0;
   fixedDirectionX_ = fixedDirection.x;
   fixedDirectionY_ = fixedDirection.y;
   UpdateConstantBuffer();
   return true;
}

nlohmann::json Dissolve::SerializeSettings() const {
   return {{"threshold", params_.threshold}, {"edgeWidth", params_.edgeWidth}, {"edgeIntensity", params_.edgeIntensity},
      {"maskContrast", params_.maskContrast}, {"maskTiling", {params_.maskTiling.x, params_.maskTiling.y}},
      {"maskOffset", {params_.maskOffset.x, params_.maskOffset.y}},
      {"edgeColor", {params_.edgeColor[0], params_.edgeColor[1], params_.edgeColor[2], params_.edgeColor[3]}},
      {"dissolveColor",
         {params_.dissolveColor[0], params_.dissolveColor[1], params_.dissolveColor[2], params_.dissolveColor[3]}},
      {"maskTexture", maskTextureName_}};
}

bool Dissolve::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }

   // 複数の関連パラメーターは一時コピーへ復元してから一括適用し、
   // セッター側の検証と GPU 更新を一度だけ通す。
   DissolveParams params = params_;
   params.threshold = GameEngine::JsonValue::ReadFloat(settings, "threshold", params.threshold);
   params.edgeWidth = GameEngine::JsonValue::ReadFloat(settings, "edgeWidth", params.edgeWidth);
   params.edgeIntensity = GameEngine::JsonValue::ReadFloat(settings, "edgeIntensity", params.edgeIntensity);
   params.maskContrast = GameEngine::JsonValue::ReadFloat(settings, "maskContrast", params.maskContrast);
   params.maskTiling = ReadVector2(settings, "maskTiling", params.maskTiling);
   params.maskOffset = ReadVector2(settings, "maskOffset", params.maskOffset);
   ReadFloat4(settings, "edgeColor", params.edgeColor);
   ReadFloat4(settings, "dissolveColor", params.dissolveColor);
   SetParams(params);
   SetMaskTextureName(ReadString(settings, "maskTexture", maskTextureName_));
   return true;
}

nlohmann::json GaussFilter::SerializeSettings() const {
   return {{"intensity", intensity_}, {"kernelSize", kernelSize_}, {"sigma", sigma_}};
}

bool GaussFilter::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }

   intensity_ = GameEngine::JsonValue::ReadFloat(settings, "intensity", intensity_);
   kernelSize_ = ReadInt32(settings, "kernelSize", kernelSize_);
   sigma_ = GameEngine::JsonValue::ReadFloat(settings, "sigma", sigma_);
   UpdateConstantBuffer();
   return true;
}

nlohmann::json Outline::SerializeSettings() const {
   return {{"outlineColor", {outlineColor_[0], outlineColor_[1], outlineColor_[2], outlineColor_[3]}},
      {"thickness", thickness_}, {"depthThreshold", depthThreshold_}, {"intensity", intensity_}};
}

bool Outline::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }

   ReadFloat4(settings, "outlineColor", outlineColor_);
   thickness_ = GameEngine::JsonValue::ReadFloat(settings, "thickness", thickness_);
   depthThreshold_ = GameEngine::JsonValue::ReadFloat(settings, "depthThreshold", depthThreshold_);
   intensity_ = GameEngine::JsonValue::ReadFloat(settings, "intensity", intensity_);
   UpdateConstantBuffer();
   return true;
}

nlohmann::json Pixelation::SerializeSettings() const {
   return {{"pixelSize", pixelSize_}};
}

bool Pixelation::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }

   pixelSize_ = GameEngine::JsonValue::ReadFloat(settings, "pixelSize", pixelSize_);
   UpdateConstantBuffer();
   return true;
}

nlohmann::json RadialBlur::SerializeSettings() const {
   return {{"center", {params_.center.x, params_.center.y}}, {"strength", params_.strength},
      {"sampleCount", params_.sampleCount}};
}

bool RadialBlur::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }

   // SetParams を経由するため、CPU 側の保持値と定数バッファが同じタイミングで切り替わる。
   RadialBlurParams params = params_;
   params.center = ReadVector2(settings, "center", params.center);
   params.strength = GameEngine::JsonValue::ReadFloat(settings, "strength", params.strength);
   params.sampleCount = ReadInt32(settings, "sampleCount", params.sampleCount);
   SetParams(params);
   return true;
}

nlohmann::json ShockWave::SerializeSettings() const {
   return {{"center", {centerX_, centerY_}}, {"aspectRatio", {aspectRatioX_, aspectRatioY_}},
      {"waveRadius", waveRadius_}, {"waveThickness", waveThickness_}, {"distortionStrength", distortionStrength_},
      {"fadeOutRadius", fadeOutRadius_}, {"highlightIntensity", highlightIntensity_}};
}

bool ShockWave::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }

   const Vector2 center = ReadVector2(settings, "center", Vector2(centerX_, centerY_));
   const Vector2 aspectRatio = ReadVector2(settings, "aspectRatio", Vector2(aspectRatioX_, aspectRatioY_));
   centerX_ = center.x;
   centerY_ = center.y;
   aspectRatioX_ = aspectRatio.x;
   aspectRatioY_ = aspectRatio.y;
   waveRadius_ = GameEngine::JsonValue::ReadFloat(settings, "waveRadius", waveRadius_);
   waveThickness_ = GameEngine::JsonValue::ReadFloat(settings, "waveThickness", waveThickness_);
   distortionStrength_ = GameEngine::JsonValue::ReadFloat(settings, "distortionStrength", distortionStrength_);
   fadeOutRadius_ = GameEngine::JsonValue::ReadFloat(settings, "fadeOutRadius", fadeOutRadius_);
   highlightIntensity_ = GameEngine::JsonValue::ReadFloat(settings, "highlightIntensity", highlightIntensity_);
   UpdateConstantBuffer();
   return true;
}

nlohmann::json SpeedLine::SerializeSettings() const {
   return {{"center", {params_.center.x, params_.center.y}}, {"intensity", params_.intensity},
      {"lineDensity", params_.lineDensity}, {"thickness", params_.thickness}, {"innerRadius", params_.innerRadius},
      {"outerRadius", params_.outerRadius}, {"time", params_.time}, {"randomSeed", params_.randomSeed},
      {"flowSpeed", params_.flowSpeed}};
}

bool SpeedLine::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }

   SpeedLineParams params = params_;
   params.center = ReadVector2(settings, "center", params.center);
   params.intensity = GameEngine::JsonValue::ReadFloat(settings, "intensity", params.intensity);
   params.lineDensity = GameEngine::JsonValue::ReadFloat(settings, "lineDensity", params.lineDensity);
   params.thickness = GameEngine::JsonValue::ReadFloat(settings, "thickness", params.thickness);
   params.innerRadius = GameEngine::JsonValue::ReadFloat(settings, "innerRadius", params.innerRadius);
   params.outerRadius = GameEngine::JsonValue::ReadFloat(settings, "outerRadius", params.outerRadius);
   params.time = GameEngine::JsonValue::ReadFloat(settings, "time", params.time);
   params.randomSeed = GameEngine::JsonValue::ReadFloat(settings, "randomSeed", params.randomSeed);
   params.flowSpeed = GameEngine::JsonValue::ReadFloat(settings, "flowSpeed", params.flowSpeed);
   SetParams(params);
   return true;
}

nlohmann::json Vignette::SerializeSettings() const {
   return {{"center", {centerX_, centerY_}}, {"radius", radius_}, {"softness", softness_},
      {"color", {vignetteColorR_, vignetteColorG_, vignetteColorB_}}, {"intensity", intensity_}};
}

bool Vignette::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }

   const Vector2 center = ReadVector2(settings, "center", Vector2(centerX_, centerY_));
   centerX_ = center.x;
   centerY_ = center.y;
   radius_ = GameEngine::JsonValue::ReadFloat(settings, "radius", radius_);
   softness_ = GameEngine::JsonValue::ReadFloat(settings, "softness", softness_);
   const auto color = settings.find("color");

   if (color != settings.end() && color->is_array() && color->size() == 3 && (*color)[0].is_number() &&
       (*color)[1].is_number() && (*color)[2].is_number()) {
      try {
         const float red = (*color)[0].get<float>();
         const float green = (*color)[1].get<float>();
         const float blue = (*color)[2].get<float>();

         if (std::isfinite(red) && std::isfinite(green) && std::isfinite(blue)) {
            vignetteColorR_ = red;
            vignetteColorG_ = green;
            vignetteColorB_ = blue;
         }
      } catch (const nlohmann::json::exception&) {
         // 数値変換が範囲外の場合は、以前の色を維持する。
      }
   }

   intensity_ = GameEngine::JsonValue::ReadFloat(settings, "intensity", intensity_);
   UpdateConstantBuffer();
   return true;
}

nlohmann::json WhiteNoise::SerializeSettings() const {
   // 時間は描画処理で進むため、意図的にシーン作成データから除外する。
   return {{"noiseDensity", params_.noiseDensity}, {"seedChangeRate", params_.seedChangeRate},
      {"noiseThreshold", params_.noiseThreshold}, {"noiseIntensity", params_.noiseIntensity}};
}

bool WhiteNoise::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }

   // 経過時間は保存データではなく実行時状態なので、ロード時に時計とともに再始動する。
   WhiteNoiseParams params = params_;
   params.time = 0.0f;
   params.noiseDensity = GameEngine::JsonValue::ReadFloat(settings, "noiseDensity", params.noiseDensity);
   params.seedChangeRate = GameEngine::JsonValue::ReadFloat(settings, "seedChangeRate", params.seedChangeRate);
   params.noiseThreshold = GameEngine::JsonValue::ReadFloat(settings, "noiseThreshold", params.noiseThreshold);
   params.noiseIntensity = GameEngine::JsonValue::ReadFloat(settings, "noiseIntensity", params.noiseIntensity);
   SetParams(params);
   previousTime_ = std::chrono::steady_clock::now();
   hasPreviousTime_ = true;
   return true;
}

} // namespace GameEngine
